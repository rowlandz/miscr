#ifndef BORROWCHECKER_ACCESSPATH
#define BORROWCHECKER_ACCESSPATH

#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/StringMap.h>
#include <llvm/ADT/DenseMap.h>
#include <llvm/Support/raw_ostream.h>

class AccessPath;
class AccessPathManager;
void dumpAccessPath(const AccessPath*);

/// @brief Represents a sequence of pointer offsets and dereferences used to
/// access a value.
///
/// AccessPath objects must be created, managed, and destroyed by an
/// AccessPathManager.
class AccessPath {
public:
  enum Tag { ROOT, STRUCT_OFFSET, ARRAY_OFFSET, DEREF };
  const Tag tag;
protected:
  AccessPath(Tag tag) : tag(tag) {}
  ~AccessPath() {}
public:
  /// @brief Dump a human-readable version of ap to stderr for debugging. A
  /// newline is also printed at the end.
  void dump() const { dumpAccessPath(this); llvm::errs() << "\n"; }
};

/// @brief The root of an access path.
class RootPath : public AccessPath {
  friend class AccessPathManager;
  std::string root;
  RootPath(llvm::StringRef root) : AccessPath(ROOT), root(root) {}
  ~RootPath() {}
public:
  static RootPath* downcast(AccessPath* ap)
    { return ap->tag == ROOT ? static_cast<RootPath*>(ap) : nullptr; }
  static const RootPath* downcast(const AccessPath* ap)
    { return ap->tag == ROOT ? static_cast<const RootPath*>(ap) : nullptr; }
  llvm::StringRef asString() const { return root; }
};

class NonRootPath : public AccessPath {
protected:
  AccessPath* base;
  NonRootPath(Tag tag, AccessPath* base) : AccessPath(tag), base(base) {}
  ~NonRootPath() {}
public:
  const AccessPath* getBase() const { return base; }
};

/// @brief An access path that ends with `[.field]`.
class StructOffsetPath : public NonRootPath {
  friend class AccessPathManager;
  std::string field;
  StructOffsetPath(AccessPath* base, llvm::StringRef field)
    : NonRootPath(STRUCT_OFFSET, base), field(field) {}
  ~StructOffsetPath() {}
public:
  static StructOffsetPath* downcast(AccessPath* ap)
    { return ap->tag == STRUCT_OFFSET ?
      static_cast<StructOffsetPath*>(ap) : nullptr; }
  static const StructOffsetPath* downcast(const AccessPath* ap)
    { return ap->tag == STRUCT_OFFSET ?
      static_cast<const StructOffsetPath*>(ap) : nullptr; }
  llvm::StringRef getField() const { return field; }
};

/// @brief An access path that ends with `[number]`.
class ArrayOffsetPath : public NonRootPath {
  friend class AccessPathManager;
  int offset;
  ArrayOffsetPath(AccessPath* base, int offset)
    : NonRootPath(ARRAY_OFFSET, base), offset(offset) {}
  ~ArrayOffsetPath() {}
public:
  static ArrayOffsetPath* downcast(AccessPath* ap)
    { return ap->tag == ARRAY_OFFSET ?
      static_cast<ArrayOffsetPath*>(ap) : nullptr; }
  static const ArrayOffsetPath* downcast(const AccessPath* ap)
    { return ap->tag == ARRAY_OFFSET ?
      static_cast<const ArrayOffsetPath*>(ap) : nullptr; }
  int getOffset() const { return offset; }
};

/// @brief An access path that ends with a dereference operator `!`.
class DerefPath : public NonRootPath {
  friend class AccessPathManager;
  DerefPath(AccessPath* base) : NonRootPath(DEREF, base) {}
  ~DerefPath() {}
public:
  static DerefPath* downcast(AccessPath* ap)
    { return ap->tag == DEREF ? static_cast<DerefPath*>(ap) : nullptr; }
  static const DerefPath* downcast(const AccessPath* ap)
    { return ap->tag == DEREF ? static_cast<const DerefPath*>(ap) : nullptr; }
};

/// @brief Manages the memory of AccessPath objects. AccessPath objects are
/// uniqued, so comparing the pointer values of two access paths should be
/// equivalent to a deep comparison.
///
/// Note: be careful with aliases.
class AccessPathManager {
public:

  AccessPathManager() {}

  ~AccessPathManager() {
    for (llvm::StringRef root : rootPaths.keys()) {
      delete rootPaths[root];
    }
    for (auto pair : nonRootPaths) {
      for (NonRootPath* nrp : pair.getSecond()) {
        if (auto path = StructOffsetPath::downcast(nrp)) {
          delete path;
        } else if (auto path = ArrayOffsetPath::downcast(nrp)) {
          delete path;
        } else if (auto path = DerefPath::downcast(nrp)) {
          delete path;
        } else llvm_unreachable("Unrecognized NonRootPath variant.");
      }
    }
  }

  /// @brief Finds or creates an access path equivalent to @p root.
  AccessPath* getRoot(llvm::StringRef root) {
    if (AccessPath* alias = aliases.lookup(root)) return alias;
    if (RootPath* path = rootPaths.lookup(root)) return path;
    RootPath* ret = new RootPath(root);
    rootPaths[root] = ret;
    return ret;
  }

  /// @brief Finds or creates an access path equivalent to `base[.field]`.
  StructOffsetPath* getStructOffset(AccessPath* base,
      llvm::StringRef field) {
    llvm::SmallVector<NonRootPath*> candidates = nonRootPaths.lookup(base);
    if (!candidates.empty()) {
      for (NonRootPath* nrp : candidates) {
        if (auto sop = StructOffsetPath::downcast(nrp)) {
          if (sop->getField() == field) return sop;
        }
      }
      StructOffsetPath* ret = new StructOffsetPath(base, field);
      candidates.push_back(ret);
      nonRootPaths[base] = candidates;
      return ret;
    } else {
      StructOffsetPath* ret = new StructOffsetPath(base, field);
      nonRootPaths[base] = { ret };
      return ret;
    }
  }

  /// @brief Finds or creates an access path equivalent to `base[offset]`.
  ArrayOffsetPath* getArrayOffset(AccessPath* base, int offset) {
    llvm::SmallVector<NonRootPath*> candidates = nonRootPaths.lookup(base);
    if (!candidates.empty()) {
      for (NonRootPath* nrp : candidates) {
        if (auto aop = ArrayOffsetPath::downcast(nrp)) {
          if (aop->getOffset() == offset) return aop;
        }
      }
      ArrayOffsetPath* ret = new ArrayOffsetPath(base, offset);
      candidates.push_back(ret);
      nonRootPaths[base] = candidates;
      return ret;
    } else {
      ArrayOffsetPath* ret = new ArrayOffsetPath(base, offset);
      nonRootPaths[base] = { ret };
      return ret;
    }
  }

  /// @brief Finds or creates an access path equivalent to `base!`.
  DerefPath* getDeref(AccessPath* base) {
    llvm::SmallVector<NonRootPath*> candidates = nonRootPaths.lookup(base);
    if (!candidates.empty()) {
      for (NonRootPath* nrp : candidates) {
        if (auto dp = DerefPath::downcast(nrp)) {
          return dp;
        }
      }
      DerefPath* ret = new DerefPath(base);
      candidates.push_back(ret);
      nonRootPaths[base] = candidates;
      return ret;
    } else {
      DerefPath* ret = new DerefPath(base);
      nonRootPaths[base] = { ret };
      return ret;
    }
  }

  bool addAlias(llvm::StringRef alias, AccessPath* ap) {
    if (rootPaths.lookup(alias)) return false;
    if (aliases.lookup(alias)) return false;
    aliases[alias] = ap;
    return true;
  }

private:

  /// @brief Stores all root paths.
  llvm::StringMap<RootPath*> rootPaths;

  /// @brief Stores all non-root paths.
  llvm::DenseMap<AccessPath*, llvm::SmallVector<NonRootPath*>> nonRootPaths;

  /// @brief Maps aliases to the access paths they alias.
  llvm::StringMap<AccessPath*> aliases;
};

/// @brief Dump a human-readable version of @p ap to stderr for debugging.
/// A newline is _not_ printed at the end.
void dumpAccessPath(const AccessPath* ap) {
  if (auto path = RootPath::downcast(ap)) {
    llvm::errs() << path->asString();
  } else if (auto path = StructOffsetPath::downcast(ap)) {
    dumpAccessPath(path->getBase());
    llvm::errs() << "[." << path->getField() << "]";
  } else if (auto path = ArrayOffsetPath::downcast(ap)) {
    dumpAccessPath(path->getBase());
    llvm::errs() << "[" << path->getOffset() << "]";
  } else if (auto path = DerefPath::downcast(ap)) {
    dumpAccessPath(path->getBase());
    llvm::errs() << "!";
  }
}

#endif