#ifndef BORROWCHECKER_HPP
#define BORROWCHECKER_HPP

#include <llvm/ADT/DenseMap.h>
#include "common/AST.hpp"
#include "common/LocatedError.hpp"
#include "common/TypeContext.hpp"
#include "borrowchecker/AccessPath.hpp"

/// @brief The borrow checker.
class BorrowChecker {
public:
  BorrowChecker(TypeContext& tc, const Ontology& ont)
    : tc(tc), ont(ont) {}

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

  /// @brief Main borrow checking function for a function. The
  /// `ownerCreations`, and `ownerUses` are reset.
  void checkFunctionDecl(FunctionDecl* funcDecl) {
    if (funcDecl->getBody() == nullptr) return;
    ownerCreations.clear();
    ownerUses.clear();
    for (auto param : funcDecl->getParameters()->asArrayRef()) {
      AccessPath* paramAPRoot = apm.getRoot(param.first->asStringRef());
      TVar paramTy = tc.freshFromTypeExp(param.second);
      for (auto ownedExt : ownedExtensionsOf(paramAPRoot, paramTy)) {
        createOwner(ownedExt, param.first->getLocation());
      }
    }

    AccessPath* retAP = check(funcDecl->getBody());
    // TODO: make helper function to narrow the location for block exps
    Location loc;
    if (auto block = BlockExp::downcast(funcDecl->getBody()))
      loc = block->getStatements()->asArrayRef().back()->getLocation();
    else
      loc = funcDecl->getBody()->getLocation();
    use(retAP, funcDecl->getBody()->getTVar(), loc);

    // If any owned references weren't used, produce an error.
    for (auto owner : ownerCreations) {
      Location use = ownerUses.lookup(owner.first);
      if (!use.exists()) {
        errors.push_back(LocatedError()
          << "Owned reference " << owner.first->asString()
          << " is never used.\n" << owner.second
        );
      }
    }
  }

  /// @brief Main borrow checking function for expressions.
  /// @return If @p _e has a reference or `data` type, the access path for
  /// the expression is returned, otherwise `nullptr`.
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
      borrow(ret, e->getRefExp()->getLocation());
      return apm.getRoot(freshInternalVar());
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
          use(argAP, arg->getTVar(), arg->getLocation());
        }
        AccessPath* ret = apm.getRoot(freshInternalVar());
        for (auto ownedExt : ownedExtensionsOf(ret, e->getTVar())) {
          createOwner(ownedExt, e->getLocation());
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
    else if (IntLit::downcast(_e)) {
      return nullptr;
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
    else if (StringLit::downcast(_e)) {
      return nullptr;
    }
    else llvm_unreachable("BorrowChecker::check(Exp*): unexpected syntax");

  }

  /// @brief Append-only stack of borrow checker errors.
  llvm::SmallVector<LocatedError> errors;

private:
  
  AccessPathManager apm;

  TypeContext& tc;

  const Ontology& ont;

  /// @brief The keyset of this map is the set of all (used or unused) owners.
  /// For building error messages, the location where each owner was created
  /// is also stored.
  llvm::DenseMap<AccessPath*, Location> ownerCreations;

  /// @brief Maps local owners to the locations where they are used.
  /// The keyset of this map must be a subset of ownerCreations.
  llvm::DenseMap<AccessPath*, Location> ownerUses;

  int nextInternalVar = 0;

  /// @brief Returns a fresh internal variable (like `$42`).
  std::string freshInternalVar()
    { return std::string("$") + std::to_string(++nextInternalVar); }

  /// @brief Marks @p ap as an owner, created at @p loc.
  void createOwner(AccessPath* ap, Location loc) {
    ownerCreations[ap] = loc;
  }

  /// @brief Returns all _owned extensions_ of @p path. An owned extension is
  /// an AP that has @p path as a prefix and where all dereferences occuring
  /// after @p path dereference _owned_ refs (not borrowed refs).
  ///
  /// When a new value is introduced into a scope, the owned extensions of that
  /// value are precisely the additional access paths that must be used before
  /// the scope closes. Function parameters and function call returns are both
  /// examples of such value introductions.
  /// @param v The type of @p path
  llvm::SmallVector<AccessPath*> ownedExtensionsOf(AccessPath* path, TVar v) {
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
        ret.append(ownedExtensionsOf(apm.getProject(path, fName, false), fTy));
      }
      return ret;
    }
    case Type::ID::OREF: {
      llvm::SmallVector<AccessPath*> ret = { path };
      ret.append(ownedExtensionsOf(apm.getDeref(path), ty.getInner()));
      return ret;
    }
    case Type::ID::NOTYPE:
      llvm_unreachable("Didn't expect NOTYPE here.");
    }
  }

  /// @brief Checks that @p ap has not been used yet and can be borrowed. It is
  /// safe to pass `nullptr` to this function.
  /// @param loc Location where @p ap is borrowed.
  void borrow(AccessPath* ap, Location loc) {
    if (ap == nullptr) return;
    Location useLoc = ownerUses.lookup(ap);
    if (useLoc.exists()) {
      errors.push_back(LocatedError()
        << "Owned reference " << ap->asString() << " is already used here:\n"
        << useLoc << "so it cannot be borrowed later:\n" << loc
      );
    }
  }

  /// @brief Uses all owned extensions of @p path.
  /// @param path If `nullptr`, then nothing is used and `{}` is returned.
  /// @param v the type of @p path
  /// @param loc location where @p path is used. For example in `myfunc(bob)`,
  /// if @p path corresponds to `bob`, then @p loc is the location of `bob`.
  /// @return The projection extensions of @p path that were used. Could be
  /// empty.
  llvm::SmallVector<AccessPath*> use(AccessPath* path, TVar v, Location loc) {
    if (path == nullptr) return {};
    llvm::SmallVector<AccessPath*> ret;
    for (auto owner : ownedExtensionsOf(path, v)) {
      if (!ownerCreations.lookup(owner).exists()) {
        errors.push_back(LocatedError()
          << "Cannot use owned reference " << owner->asString()
          << " locked behind a borrow.\n" << loc
        );
      }
      Location useLoc = ownerUses.lookup(owner);
      if (useLoc.exists()) {
        errors.push_back(LocatedError()
          << "Owned reference " << owner->asString()
          << " is already used here:\n" << useLoc
          << " so it cannot be used later:\n" << loc
        );
      } else {
        ownerUses[owner] = loc;
        ret.push_back(owner);
      }
    }
    return ret;
  }
};

#endif