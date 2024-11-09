#ifndef BORROWCHECKER_HPP
#define BORROWCHECKER_HPP

#include <llvm/ADT/DenseMap.h>
#include "common/AST.hpp"
#include "common/LocatedError.hpp"
#include "common/TypeContext.hpp"
#include "borrowchecker/AccessPath.hpp"

/// @brief The borrow checker. Responsible for ensuring that owned refs are
/// used once and moved refs are replaced.
class BorrowChecker {
public:
  BorrowChecker(TypeContext& tc, const Ontology& ont) : tc(tc), ont(ont) {}

  /// @brief Borrow-check a declaration.
  void checkDecl(Decl* d) {
    if (auto funcDecl = FunctionDecl::downcast(d))
      checkFunctionDecl(funcDecl);
    else if (auto modDecl = ModuleDecl::downcast(d))
      checkModuleDecl(modDecl);
    else if (DataDecl::downcast(d))
      {}
    else
      llvm_unreachable("BorrowChecker::checkDecl(): unrecognized decl form.");
  }

  /// @brief Borrow-check multiple declarations. 
  void checkDecls(DeclList* decls) {
    for (auto decl : decls->asArrayRef()) checkDecl(decl);
  }

  /// @brief Borrow-check a module. 
  void checkModuleDecl(ModuleDecl* m) {
    checkDecls(m->getDecls());
  }

  /// @brief Main borrow checking function for a function. The `unusedPaths`,
  /// `usedPaths`, and `apm` are cleared.
  void checkFunctionDecl(FunctionDecl* funcDecl) {
    Exp* body = funcDecl->getBody();
    if (body == nullptr) return;
    unusedPaths.clear();
    usedPaths.clear();
    apm.clear();
    for (auto param : funcDecl->getParameters()->asArrayRef()) {
      AccessPath* paramAPRoot = apm.getRoot(param.first->asStringRef());
      TVar paramTy = tc.freshFromTypeExp(param.second);
      for (auto looseExt : looseExtensionsOf(paramAPRoot, paramTy)) {
        unusedPaths[looseExt] = param.first->getLocation();
      }
    }

    AccessPath* retAP = check(body);
    // TODO: make helper function to narrow the location for block exps
    Location loc;
    if (auto block = BlockExp::downcast(body))
      loc = block->getStatements()->asArrayRef().back()->getLocation();
    else
      loc = body->getLocation();
    for (AccessPath* ext : looseExtensionsOf(retAP, body->getTVar()))
      use(ext, loc);

    // produce errors for any remaining unused paths
    for (auto unused : unusedPaths)
      errors.push_back(LocatedError()
        << "Owned reference " << unused.first->asString()
        << " is never used.\n" << unused.second
      );

    // produce errors for any remaining moved paths
    for (auto moved : movedPaths)
      errors.push_back(LocatedError()
        << "Moved value " << moved.first->asString() << " is never replaced.\n"
        << moved.second
      );
  }

  /// @brief Main borrow checking function for expressions. Recursively
  /// traverses @p _e to perform a symbolic evaluation using access paths as
  /// the symbolic values.
  /// @return If @p _e has a reference or `data` type, the access path for
  /// the expression is returned, otherwise nullptr.
  AccessPath* check(Exp* _e) {

    if (auto e = AscripExp::downcast(_e)) {
      return check(e->getAscriptee());
    }
    else if (auto e = BinopExp::downcast(_e)) {
      check(e->getLHS());
      check(e->getRHS());
      return nullptr;
    }
    else if (auto e = BlockExp::downcast(_e)) {
      AccessPath* ret = nullptr;
      for (auto stmt : e->getStatements()->asArrayRef())
        ret = check(stmt);
      return ret;
    }
    else if (BoolLit::downcast(_e)) {
      return nullptr;
    }
    else if (auto e = BorrowExp::downcast(_e)) {
      AccessPath* ret = check(e->getRefExp());
      for (auto owner : looseExtensionsOf(ret, e->getRefExp()->getTVar())) {
        if (LocationPair locs = usedPaths.lookup(owner)) {
          errors.push_back(LocatedError()
            << "Owned reference " << owner->asString() << " created here:\n"
            << locs.fst << "is already used here:\n" << locs.snd
            << "so it cannot be borrowed later:\n"
            << e->getRefExp()->getLocation()
          );
        }
      }
      return ret;
    }
    else if (auto e = CallExp::downcast(_e)) {
      if (e->isConstr()) {
        AccessPath* ret = apm.getRoot(freshInternalVar());
        DataDecl* dataDecl = ont.getType(e->getFunction()->asStringRef());
        llvm::ArrayRef<Exp*> args = e->getArguments()->asArrayRef();
        auto fields = dataDecl->getFields()->asArrayRef();
        assert(args.size() == fields.size());
        for (int i = 0; i < args.size(); ++i) {
          llvm::StringRef fieldName = fields[i].first->asStringRef();
          AccessPath* argAP = check(args[i]);
          if (argAP != nullptr) {
            apm.aliasProject(ret, fieldName, false, argAP);
          }
        }
        return ret;
      } else {
        for (auto arg : e->getArguments()->asArrayRef()) {
          AccessPath* argAP = check(arg);
          for (AccessPath* ext : looseExtensionsOf(argAP, arg->getTVar())) {
            use(ext, arg->getLocation());
          }
        }
        AccessPath* ret = apm.getRoot(freshInternalVar());
        for (auto looseExt : looseExtensionsOf(ret, e->getTVar())) {
          unusedPaths[looseExt] = e->getLocation();
        }
        return ret;
      }
    }
    else if (DecimalLit::downcast(_e)) {
      return nullptr;
    }
    else if (auto e = DerefExp::downcast(_e)) {
      AccessPath* ofAP = check(e->getOf());
      return apm.getDeref(ofAP);
    }
    else if (auto e = IndexExp::downcast(_e)) {
      AccessPath* baseAP = check(e->getBase());
      if (auto nameIdx = NameExp::downcast(e->getIndex())) {
        return apm.getIndex(baseAP, nameIdx->getName()->asStringRef());
      } else {
        check(e->getIndex());
        errors.push_back(LocatedError()
          << "Borrow checker only supports identifier indices.\n"
          << e->getIndex()->getLocation()
        );
        return apm.getRoot(freshInternalVar());
      }
    }
    else if (IntLit::downcast(_e)) {
      return nullptr;
    }
    else if (auto e = MoveExp::downcast(_e)) {
      AccessPath* refAP = check(e->getRefExp());
      auto loosePaths = looseExtensionsOf(apm.getDeref(refAP), e->getTVar());
      for (AccessPath* loosePath : loosePaths) {
        move(loosePath, e->getRefExp()->getLocation());
      }
      AccessPath* ret = apm.getRoot(freshInternalVar());
      unusedPaths[ret] = e->getLocation();
      return ret;
    }
    else if (auto e = NameExp::downcast(_e)) {
      return apm.getRoot(e->getName()->asStringRef());
    }
    else if (auto e = LetExp::downcast(_e)) {
      Exp* def = e->getDefinition();
      AccessPath* defAP = check(def);
      if (defAP != nullptr)
        { apm.aliasRoot(e->getBoundIdent()->asStringRef(), defAP); }
      return defAP;
    }
    else if (auto e = ProjectExp::downcast(_e)) {
      AccessPath* baseAP = check(e->getBase());
      llvm::StringRef field = e->getFieldName()->asStringRef();
      return apm.getProject(baseAP, field, e->isAddrCalc());
    }
    else if (auto e = RefExp::downcast(_e)) {
      Exp* initExp = e->getInitializer();
      AccessPath* initAP = check(initExp);
      AccessPath* ret = apm.getRoot(freshInternalVar());
      if (initAP != nullptr) apm.aliasDeref(ret, initAP);
      return ret;
    }
    else if (auto e = StoreExp::downcast(_e)) {
      AccessPath* lhsAP = check(e->getLHS());
      AccessPath* rhsAP = check(e->getRHS());
      TVar rhsTVar = e->getRHS()->getTVar();
      auto lhsLooseExts = looseExtensionsOf(apm.getDeref(lhsAP), rhsTVar);
      auto rhsLooseExts = looseExtensionsOf(rhsAP, rhsTVar);
      for (AccessPath* ext : rhsLooseExts)
        use(ext, e->getRHS()->getLocation());
      for (AccessPath* ext : lhsLooseExts)
        replace(ext, e->getLHS()->getLocation());
      return nullptr;
    }
    else if (StringLit::downcast(_e)) {
      return nullptr;
    }
    else llvm_unreachable("BorrowChecker::check(Exp*): unexpected syntax");

  }

  /// @brief Append-only stack of borrow checker errors.
  llvm::SmallVector<LocatedError> errors;

private:
  
  /// @brief A pair of locations that is default-constructable so it can easily
  /// work with llvm::DenseMap.
  class LocationPair {
  public:
    Location fst;
    Location snd;
    LocationPair() {}
    LocationPair(Location fst, Location snd) : fst(fst), snd(snd) {}
    operator bool() const { return fst && snd; }
  };

  AccessPathManager apm;

  TypeContext& tc;

  const Ontology& ont;

  /// @brief All _unused_ paths with their creation locations.
  llvm::DenseMap<AccessPath*, Location> unusedPaths;

  /// @brief All _used_ paths with their creation and use locations.
  llvm::DenseMap<AccessPath*, LocationPair> usedPaths;

  /// @brief All _moved_ paths with their move locations.
  llvm::DenseMap<AccessPath*, Location> movedPaths;

  int nextInternalVar = 0;

  /// @brief Returns a fresh internal variable (like `$42`).
  std::string freshInternalVar()
    { return "$" + std::to_string(++nextInternalVar); }

  /// @brief Returns all _loose extensions_ of @p path. A loose extension is
  /// an AP that has @p path as a prefix and where all dereferences occurring
  /// after @p path dereference _owned_ refs (not borrowed refs).
  ///
  /// When a new value is introduced into a scope, the loose extensions of that
  /// value are precisely the additional access paths that must be used before
  /// the scope closes. Function parameters and function call returns are both
  /// examples of such value introductions.
  /// @param path If nullptr, an empty vector is returned.
  /// @param v The type of @p path
  llvm::SmallVector<AccessPath*> looseExtensionsOf(AccessPath* path, TVar v) {
    if (path == nullptr) return llvm::SmallVector<AccessPath*>();
    Type ty = tc.resolve(v).second;
    switch (ty.getID()) {
    case Type::ID::BOOL: case Type::ID::BREF: case Type::ID::DECIMAL:
    case Type::ID::f32: case Type::ID::f64: case Type::ID::i8:
    case Type::ID::i16: case Type::ID::i32: case Type::ID::i64:
    case Type::ID::NUMERIC: case Type::ID::UNIT:
      return {};
    case Type::ID::NAME: {
      llvm::SmallVector<AccessPath*> ret;
      DataDecl* dataDecl = ont.getType(ty.getName()->asStringRef());
      for (auto field : dataDecl->getFields()->asArrayRef()) {
        llvm::StringRef fName = field.first->asStringRef();
        TVar fTy = tc.freshFromTypeExp(field.second);
        // TODO: a recursive type would cause a stack overflow.
        ret.append(looseExtensionsOf(apm.getProject(path, fName, false), fTy));
      }
      return ret;
    }
    case Type::ID::OREF: {
      llvm::SmallVector<AccessPath*> ret = { path };
      ret.append(looseExtensionsOf(apm.getDeref(path), ty.getInner()));
      return ret;
    }
    case Type::ID::NOTYPE:
      llvm_unreachable("Didn't expect NOTYPE here.");
    }
  }

  /// @brief Moves unused path @p owner to the `usedPaths` map.
  /// @param loc The source code location where @p owner is used.
  /// @return True iff the use was successful. Otherwise an error is pushed.
  bool use(AccessPath* owner, Location loc) {
    assert(owner != nullptr && "Tried to use nullptr");
    if (Location creationLoc = unusedPaths.lookup(owner)) {
      usedPaths[owner] = LocationPair(creationLoc, loc);
      unusedPaths.erase(owner);
      return true;
    }
    else if (LocationPair locs = usedPaths.lookup(owner)) {
      errors.push_back(LocatedError()
        << "Owned reference " << owner->asString()
        << " is already used here:\n" << locs.snd
        << "so it cannot be used later:\n" << loc
      );
      return false;
    }
    else {
      errors.push_back(LocatedError()
        << "Cannot use owned reference " << owner->asString()
        << " created outside this scope.\n" << loc
      );
      return false;
    }
  }

  /// @brief Performs a _move_ on @p path. Specifically, moves @p path into the
  /// `movedPaths` map.
  /// @param moveLoc Location of `e` in `move e`.
  /// @return true iff the move was successful.
  bool move(AccessPath* path, Location moveLoc) {
    if (Location creatLoc = unusedPaths.lookup(path)) {
      errors.push_back(LocatedError()
        << "Owned reference " << path->asString() << " created here:\n"
        << creatLoc << "cannot be moved in the same scope:\n" << moveLoc
      );
      return false;
    }
    if (LocationPair locs = usedPaths.lookup(path)) {
      errors.push_back(LocatedError()
        << "Owned reference " << path->asString() << " created here:\n"
        << locs.fst << "cannot be moved in the same scope.\n" << moveLoc
      );
    }
    if (Location prevMoveLoc = movedPaths.lookup(path)) {
      errors.push_back(LocatedError()
        << "Owned reference " << path->asString()
        << " was already moved here:\n" << prevMoveLoc
        << "so it cannot be moved later:\n" << moveLoc
      );
      return false;
    }
    movedPaths[path] = moveLoc;
    return true;
  }

  /// @brief Replaces the value of a moved @p path (via a store expression).
  /// @param loc location of the LHS of a store expression
  /// @return True iff the replacement was successful.
  bool replace(AccessPath* path, Location loc) {
    if (movedPaths.lookup(path).exists()) {
      movedPaths.erase(path);
      return true;
    }
    errors.push_back(LocatedError()
      << "Owned reference " << path->asString()
      << " becomes inaccessible after store.\n" << loc
    );
    return false;
  }
};

#endif