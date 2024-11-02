#ifndef BORROWCHECKER_ACCESSPATH
#define BORROWCHECKER_ACCESSPATH

#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/StringMap.h>
#include <llvm/ADT/DenseMap.h>
#include <llvm/Support/raw_ostream.h>

class AccessPath;
class AccessPathManager;

/// @brief Represents a sequence of struct projections and dereferences used to
/// access a value.
///
/// An example is something like `myval.field1!.field2!`
///
/// AccessPath objects must be created, managed, and destroyed by an
/// AccessPathManager.
class AccessPath {
public:
  enum Tag { ROOT, PROJECTION, ARRAY_OFFSET, DEREF };
  const Tag tag;
protected:
  AccessPath(Tag tag) : tag(tag) {}
  ~AccessPath() {}
public:

  /// @brief Returns this access path as a string for error messages.
  std::string asString() const;

  /// @brief Returns true iff this access path begins with @p prefix.
  bool startsWith(const AccessPath* prefix) const;
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
  AccessPath* getBase() const { return base; }

  static const NonRootPath* downcast(const AccessPath* ap) {
    switch (ap->tag) {
    case PROJECTION: case ARRAY_OFFSET: case DEREF:
      return static_cast<const NonRootPath*>(ap);
    default: return nullptr;
    }
  }

  static NonRootPath* downcast(AccessPath* ap) {
    switch (ap->tag) {
    case PROJECTION: case ARRAY_OFFSET: case DEREF:
      return static_cast<NonRootPath*>(ap);
    default: return nullptr;
    }
  }
};

/// @brief An access path that ends with `.field`.
class ProjectionPath : public NonRootPath {
  friend class AccessPathManager;
  std::string field;
  ProjectionPath(AccessPath* base, llvm::StringRef field)
    : NonRootPath(PROJECTION, base), field(field) {}
  ~ProjectionPath() {}
public:
  static ProjectionPath* downcast(AccessPath* ap)
    { return ap->tag == PROJECTION ?
      static_cast<ProjectionPath*>(ap) : nullptr; }
  static const ProjectionPath* downcast(const AccessPath* ap)
    { return ap->tag == PROJECTION ?
      static_cast<const ProjectionPath*>(ap) : nullptr; }
  llvm::StringRef getField() const { return field; }
};

/// @brief The equivalent of struct projection, but for arrays (whose "fields"
/// are simply nonnegative integers).
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
        if (auto path = ProjectionPath::downcast(nrp)) {
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

  /// @brief Finds or creates an access path equivalent to `base.field`.
  ProjectionPath* getProjection(AccessPath* base,
      llvm::StringRef field) {
    llvm::SmallVector<NonRootPath*> candidates = nonRootPaths.lookup(base);
    if (!candidates.empty()) {
      for (NonRootPath* nrp : candidates) {
        if (auto sop = ProjectionPath::downcast(nrp)) {
          if (sop->getField() == field) return sop;
        }
      }
      ProjectionPath* ret = new ProjectionPath(base, field);
      candidates.push_back(ret);
      nonRootPaths[base] = candidates;
      return ret;
    } else {
      ProjectionPath* ret = new ProjectionPath(base, field);
      nonRootPaths[base] = { ret };
      return ret;
    }
  }

  /// @brief Finds or creates an access path equivalent to `base.offset`.
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

  /// @brief Gets the access path that is the same as @p of except with
  /// @p prefix replaced with @p with.
  /// @return If @p prefix is not a prefix of @p of, returns `nullptr`. 
  AccessPath* replacePrefix(AccessPath* of, AccessPath* prefix,
      AccessPath* with) {
    if (of == prefix) return with;
    if (RootPath::downcast(of)) return nullptr;
    else if (auto pp = ProjectionPath::downcast(of)) {
      AccessPath* ret = replacePrefix(pp->getBase(), prefix, with);
      if (ret == nullptr) return nullptr;
      return getProjection(ret, pp->getField());
    } else if (auto aop = ArrayOffsetPath::downcast(of)) {
      AccessPath* ret = replacePrefix(aop->getBase(), prefix, with);
      if (ret == nullptr) return nullptr;
      return getArrayOffset(ret, aop->getOffset());
    } else if (auto dp = DerefPath::downcast(of)) {
      AccessPath* ret = replacePrefix(dp->getBase(), prefix, with);
      if (ret == nullptr) return nullptr;
      return getDeref(ret);
    } else llvm_unreachable("unexpected case");
  }

private:

  /// @brief Stores all root paths.
  llvm::StringMap<RootPath*> rootPaths;

  /// @brief Stores all non-root paths.
  llvm::DenseMap<AccessPath*, llvm::SmallVector<NonRootPath*>> nonRootPaths;

  /// @brief Maps aliases to the access paths they alias.
  llvm::StringMap<AccessPath*> aliases;
};

std::string AccessPath::asString() const {
  if (auto path = RootPath::downcast(this)) {
    return path->asString().str();
  } else if (auto path = ProjectionPath::downcast(this)) {
    std::string ret = path->getBase()->asString();
    ret += ".";
    ret += path->getField();
    return ret;
  } else if (auto path = ArrayOffsetPath::downcast(this)) {
    std::string ret = path->getBase()->asString();
    ret += ".";
    ret += path->getOffset();
    return ret;
  } else if (auto path = DerefPath::downcast(this)) {
    std::string ret = path->getBase()->asString();
    ret += "!";
    return ret;
  } else llvm_unreachable("accessPathAsString: unexpected case");
}

bool AccessPath::startsWith(const AccessPath* prefix) const {
  const AccessPath* this_ = this;
  for (;;) {
    if (this_ == prefix) return true;
    if (RootPath::downcast(this_))
      return false;
    else if (auto nrp = NonRootPath::downcast(this_))
      this_ = nrp->getBase();
    else llvm_unreachable("unrecognized case");
  }
}

#endif