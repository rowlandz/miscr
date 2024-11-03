#ifndef BORROWCHECKER_ACCESSPATH
#define BORROWCHECKER_ACCESSPATH

#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/StringMap.h>
#include <llvm/ADT/DenseMap.h>
#include <llvm/Support/raw_ostream.h>

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
  enum Tag { ROOT, PROJECT, ARRAY_OFFSET, DEREF };
  const Tag tag;
protected:
  AccessPath(Tag tag) : tag(tag) {}
  ~AccessPath() {}
public:

  /// @brief Returns this access path as a string for error messages.
  std::string asString();

  /// @brief Returns true iff this access path begins with @p prefix.
  bool startsWith(const AccessPath* prefix);
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
  llvm::StringRef asString() const { return root; }
};

class NonRootPath : public AccessPath {
protected:
  AccessPath* base;
  NonRootPath(Tag tag, AccessPath* base) : AccessPath(tag), base(base) {}
  ~NonRootPath() {}
public:
  AccessPath* getBase() const { return base; }

  static NonRootPath* downcast(AccessPath* ap) {
    switch (ap->tag) {
    case PROJECT: case ARRAY_OFFSET: case DEREF:
      return static_cast<NonRootPath*>(ap);
    default: return nullptr;
    }
  }
};

/// @brief An access path that ends with `.field` or `[.field]`. Corresponds
/// with ProjectExp.
///
/// It is important that access paths be structurally unique. ProjectPath
/// presents a problem because `base!.field` is the same as `base[.field]!`.
/// Therefore we decree that any access path that contains `[.field]` followed
/// by a deref, `.field` projection, or `.index` projection is invalid. The
/// AccessPathManager is responsible for performing the necessary
/// transformations to enforce this.
class ProjectPath : public NonRootPath {
  friend class AccessPathManager;
  std::string field;
  bool isAddrCalc_;
  ProjectPath(AccessPath* base, llvm::StringRef field, bool isAddrCalc)
    : NonRootPath(PROJECT, base), field(field), isAddrCalc_(isAddrCalc) {}
  ~ProjectPath() {}
public:
  static ProjectPath* downcast(AccessPath* ap)
    { return ap->tag == PROJECT ? static_cast<ProjectPath*>(ap) : nullptr; }
  llvm::StringRef getField() const { return field; }
  bool isAddrCalc() const { return isAddrCalc_; }
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
};

/// @brief Manages the memory of AccessPath objects. AccessPath objects are
/// uniqued, so comparing the pointer values of two access paths should be
/// equivalent to a deep comparison.
class AccessPathManager {
public:

  AccessPathManager() {}

  ~AccessPathManager() {
    for (llvm::StringRef root : rootPaths.keys()) {
      delete rootPaths[root];
    }
    for (auto pair : nonRootPaths) {
      for (NonRootPath* nrp : pair.getSecond()) {
        if (auto path = ProjectPath::downcast(nrp)) {
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
    if (RootPath* path = rootPaths.lookup(root)) return path;
    RootPath* ret = new RootPath(root);
    rootPaths[root] = ret;
    return ret;
  }

  /// @brief Finds or creates an access path equivalent to `base.field` or
  /// `base[.field]`.
  ProjectPath*
  getProject(AccessPath* base, llvm::StringRef field, bool isAddrCalc) {
    if (!isAddrCalc) {
      if (auto baseProjectExp = ProjectPath::downcast(base))
        { assert(!baseProjectExp->isAddrCalc()); }
    }
    llvm::SmallVector<NonRootPath*> candidates = nonRootPaths.lookup(base);
    for (NonRootPath* nrp : candidates) {
      if (auto sop = ProjectPath::downcast(nrp)) {
        if (sop->isAddrCalc() == isAddrCalc && sop->getField() == field)
          return sop;
      }
    }
    ProjectPath* ret = new ProjectPath(base, field, isAddrCalc);
    candidates.push_back(ret);
    nonRootPaths[base] = candidates;
    return ret;
  }

  /// @brief Finds or creates an access path equivalent to `base[.field]`.
  ProjectPath* getProjectAddrCalc(AccessPath* base, llvm::StringRef field)
    { return getProject(base, field, true); }

  /// @brief Finds or creates an access path equivalent to `base.offset`.
  ArrayOffsetPath* getArrayOffset(AccessPath* base, int offset) {
    if (auto baseProjectExp = ProjectPath::downcast(base))
      { assert(!baseProjectExp->isAddrCalc()); }
    llvm::SmallVector<NonRootPath*> candidates = nonRootPaths.lookup(base);
    for (NonRootPath* nrp : candidates) {
      if (auto aop = ArrayOffsetPath::downcast(nrp)) {
        if (aop->getOffset() == offset) return aop;
      }
    }
    ArrayOffsetPath* ret = new ArrayOffsetPath(base, offset);
    candidates.push_back(ret);
    nonRootPaths[base] = candidates;
    return ret;
  }

  /// @brief Finds or creates an access path equivalent to `base!`.
  NonRootPath* getDeref(AccessPath* base) {
    
    // transform `basebase[.field]!` into `basebase!.field` if possible
    if (auto baseProject = ProjectPath::downcast(base)) {
      if (baseProject->isAddrCalc()) {
        AccessPath* basebase = baseProject->getBase();
        return getProject(getDeref(basebase), baseProject->getField(), false);
      }
    }

    // otherwise append the `!` like normal
    llvm::SmallVector<NonRootPath*> candidates = nonRootPaths.lookup(base);
    for (NonRootPath* nrp : candidates) {
      if (DerefPath::downcast(nrp)) return nrp;
    }
    DerefPath* ret = new DerefPath(base);
    candidates.push_back(ret);
    nonRootPaths[base] = candidates;
    return ret;
  }

  /// @brief Gets the access path that is the same as @p of except with
  /// @p prefix replaced with @p with.
  /// @return If @p prefix is not a prefix of @p of, returns `nullptr`. 
  AccessPath*
  replacePrefix(AccessPath* of, AccessPath* prefix, AccessPath* with) {
    if (of == prefix) return with;
    if (RootPath::downcast(of)) return nullptr;
    else if (auto pp = ProjectPath::downcast(of)) {
      AccessPath* ret = replacePrefix(pp->getBase(), prefix, with);
      if (ret == nullptr) return nullptr;
      return getProject(ret, pp->getField(), pp->isAddrCalc());
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

  /// @brief Stores all non-root paths. For uniquing purposes, non-root paths
  /// are indexed by their prefix.
  llvm::DenseMap<AccessPath*, llvm::SmallVector<NonRootPath*>> nonRootPaths;
};

std::string AccessPath::asString() {
  if (auto path = RootPath::downcast(this)) {
    return path->asString().str();
  } else if (auto path = ProjectPath::downcast(this)) {
    if (path->isAddrCalc()) {
      std::string ret = path->getBase()->asString();
      ret += "[.";
      ret += path->getField();
      ret += "]";
      return ret;
    } else {
      std::string ret = path->getBase()->asString();
      ret += ".";
      ret += path->getField();
      return ret;
    }
  } else if (auto path = ArrayOffsetPath::downcast(this)) {
    std::string ret = path->getBase()->asString();
    ret += ".";
    ret += path->getOffset();
    return ret;
  } else if (auto path = DerefPath::downcast(this)) {
    std::string ret = path->getBase()->asString();
    ret += "!";
    return ret;
  } else llvm_unreachable("AccessPath::asString: unexpected case");
}

bool AccessPath::startsWith(const AccessPath* prefix) {
  AccessPath* this_ = this;
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