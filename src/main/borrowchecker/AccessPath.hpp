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
/// AccessPath objects are uniqued and therefore must be created, managed, and
/// destroyed by an AccessPathManager.
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

/// @brief Enforces the uniqueness of AccessPath objects.
///
/// AccessPaths are _uniqued_, meaning that any two distinct AccessPath objects
/// cannot be structurally equivalent. This enables structural equivalence to
/// be checked using simple pointer comparison. To achieve this, all creation
/// of access paths must be done via the `get` methods of this class.
///
/// All access paths are eagerly reduced to a "standard form" by applying the
/// following two rules whenever possible:
///   - `prefix[.field]!` is replaced with `prefix!.field`
///   - aliases are replaced with their expansions
///
/// Access paths of the form `prefix[.field1].field2` or `prefix[.field].offset`
/// must _never_ be created; immediate termination will occur if an attempt is
/// made to do so.
class AccessPathManager {
public:
  AccessPathManager() {}
  ~AccessPathManager() { clear(); }
  AccessPathManager(const AccessPathManager&) = delete;
  AccessPathManager& operator=(const AccessPathManager&) = delete;

  /// @brief Finds the access path @p root if it has been created before.
  /// Otherwise returns nullptr. 
  AccessPath* findRoot(llvm::StringRef root) const {
    if (RootPath* path = rootPaths.lookup(root)) return dealias(path);
    return nullptr;
  }

  /// @brief Finds the access path equivalent to `base.field` or `base[.field]`
  /// if it has been created before. Otherwise returns nullptr.
  AccessPath*
  findProject(AccessPath* base, llvm::StringRef field, bool isAddrCalc) const {
    if (!isAddrCalc) {
      if (auto baseProjectExp = ProjectPath::downcast(base))
        { assert(!baseProjectExp->isAddrCalc()); }
    }
    llvm::SmallVector<NonRootPath*> candidates = nonRootPaths.lookup(base);
    for (NonRootPath* nrp : candidates) {
      if (auto sop = ProjectPath::downcast(nrp)) {
        if (sop->isAddrCalc() == isAddrCalc && sop->getField() == field)
          return dealias(sop);
      }
    }
    return nullptr;
  }

  /// @brief Finds the access path equivalent to `base.offset` if it has been
  /// created before. Otherwise returns nullptr.
  AccessPath* findArrayOffset(AccessPath* base, int offset) const {
    if (auto baseProjectExp = ProjectPath::downcast(base))
      { assert(!baseProjectExp->isAddrCalc()); }
    llvm::SmallVector<NonRootPath*> candidates = nonRootPaths.lookup(base);
    for (NonRootPath* nrp : candidates) {
      if (auto aop = ArrayOffsetPath::downcast(nrp)) {
        if (aop->getOffset() == offset) return dealias(aop);
      }
    }
    return nullptr;
  }

  /// @brief Finds the access path equivalent to `base!` if created before.
  /// Otherwise returns nullptr.
  AccessPath* findDeref(AccessPath* base) const {
    
    // find `basebase!.field` instead of `basebase[.field]!` if applicable
    if (auto baseProject = ProjectPath::downcast(base)) {
      if (baseProject->isAddrCalc()) {
        AccessPath* basebase = baseProject->getBase();
        AccessPath* basebaseDeref = findDeref(basebase);
        if (basebaseDeref == nullptr) return nullptr;
        return findProject(basebaseDeref, baseProject->getField(), false);        
      }
    }

    // otherwise by prefix like normal
    llvm::SmallVector<NonRootPath*> candidates = nonRootPaths.lookup(base);
    for (NonRootPath* nrp : candidates) {
      if (DerefPath::downcast(nrp)) return dealias(nrp);
    }
    return nullptr;
  }

  /// @brief Finds or creates an access path equivalent to @p root.
  AccessPath* getRoot(llvm::StringRef root) {
    if (AccessPath* ret = findRoot(root)) return ret;
    RootPath* ret = new RootPath(root);
    rootPaths[root] = ret;
    return ret;
  }

  /// @brief Finds or creates an access path equivalent to `base.field` or
  /// `base[.field]`.
  AccessPath*
  getProject(AccessPath* base, llvm::StringRef field, bool isAddrCalc) {
    if (AccessPath* ret = findProject(base, field, isAddrCalc)) return ret;
    ProjectPath* ret = new ProjectPath(base, field, isAddrCalc);
    nonRootPaths[base].push_back(ret);
    return ret;
  }

  /// @brief Finds or creates an access path equivalent to `base.offset`.
  AccessPath* getArrayOffset(AccessPath* base, int offset) {
    if (AccessPath* ret = findArrayOffset(base, offset)) return ret;
    ArrayOffsetPath* ret = new ArrayOffsetPath(base, offset);
    nonRootPaths[base].push_back(ret);
    return ret;
  }

  /// @brief Finds or creates an access path equivalent to `base!`.
  AccessPath* getDeref(AccessPath* base) {
    if (AccessPath* ret = findDeref(base)) return ret;

    // transform `basebase[.field]!` into `basebase!.field` if possible
    if (auto baseProject = ProjectPath::downcast(base)) {
      if (baseProject->isAddrCalc()) {
        AccessPath* basebase = baseProject->getBase();
        return getProject(getDeref(basebase), baseProject->getField(), false);
      }
    }

    DerefPath* ret = new DerefPath(base);
    nonRootPaths[base].push_back(ret);
    return ret;
  }

  /// @brief Sets access path `root` as an alias for @p expansion. `root` must
  /// not be created yet.
  void aliasRoot(llvm::StringRef root, AccessPath* expansion) {
    assert(findRoot(root) == nullptr && "existing root path cannot be alias");
    RootPath* alias = new RootPath(root);
    rootPaths[root] = alias;
    rewrites[alias] = expansion;
  }

  /// @brief Sets access path `base.field` or `base[.field]` as an alias for
  /// @p expansion. The alias must not be created yet.
  void aliasProject(AccessPath* base, llvm::StringRef field, bool isAddrCalc,
                    AccessPath* expansion) {
    assert(findProject(base, field, isAddrCalc) == nullptr);
    ProjectPath* alias = new ProjectPath(base, field, isAddrCalc);
    nonRootPaths[base].push_back(alias);
    rewrites[alias] = expansion;
  }

  /// @brief Sets access path `base.offset` as an alias for @p expansion.
  /// `base.offset` must not be created yet.
  void aliasArrayOffset(AccessPath* base, int offset, AccessPath* expansion) {
    assert(findArrayOffset(base, offset) == nullptr);
    ArrayOffsetPath* alias = new ArrayOffsetPath(base, offset);
    nonRootPaths[base].push_back(alias);
    rewrites[alias] = expansion;
  }

  /// @brief Sets access path `base!` as an alias for @p expansion. `base!`
  /// must not be created yet.
  void aliasDeref(AccessPath* base, AccessPath* expansion) {
    assert(findDeref(base) == nullptr && "existing deref path cannot be alias");
    DerefPath* alias = new DerefPath(base);
    nonRootPaths[base].push_back(alias);
    rewrites[alias] = expansion;
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

  /// @brief Resets this manager object back to its initially-created state.
  void clear() {
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

private:

  /// @brief Stores all root paths.
  llvm::StringMap<RootPath*> rootPaths;

  /// @brief Stores all non-root paths. For uniquing purposes, non-root paths
  /// are indexed by their prefix.
  llvm::DenseMap<AccessPath*, llvm::SmallVector<NonRootPath*>> nonRootPaths;

  /// @brief Maps alias paths to their expansions. The key-set and value-set
  /// must be disjoint. No alias should ever escape this class.
  llvm::DenseMap<AccessPath*, AccessPath*> rewrites;

  /// @brief Resolves @p path if it is an alias, or returns @p path if not.
  AccessPath* dealias(AccessPath* path) const {
    if (AccessPath* ret = rewrites.lookup(path)) return ret;
    return path;
  }
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