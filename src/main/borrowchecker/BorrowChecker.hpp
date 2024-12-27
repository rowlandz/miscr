#ifndef BORROWCHECKER_HPP
#define BORROWCHECKER_HPP

#include <llvm/ADT/DenseMap.h>
#include "common/AST.hpp"
#include "common/LocatedError.hpp"
#include "common/TypeContext.hpp"
#include "borrowchecker/AccessPath.hpp"
#include "borrowchecker/BorrowState.hpp"

/// @brief The borrow checker. Responsible for ensuring that unique refs are
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
    else if (StructDecl::downcast(d))
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
    apm.clear();
    BorrowState block(errors);
    for (auto param : funcDecl->getParameters()->asArrayRef()) {
      AccessPath* paramAPRoot = apm.getRoot(param.first->asStringRef());
      Type* paramTy = tc.getTypeFromTypeExp(param.second);
      for (auto looseExt : looseExtensionsOf(paramAPRoot, paramTy)) {
        block.intro(looseExt, param.first->getLocation());
      }
    }

    bs = &block;
    AccessPath* retAP = check(body);
    bs = nullptr;

    // TODO: make helper function to narrow the location for block exps
    Location loc;
    if (auto block = BlockExp::downcast(body))
      loc = block->getStatements()->asArrayRef().back()->getLocation();
    else
      loc = body->getLocation();
    for (AccessPath* ext : looseExtensionsOf(retAP, body->getType()))
      block.use(ext, loc);

    // produce errors for any remaining unused paths
    for (auto unused : block.getUnusedPaths())
      errors.push_back(LocatedError()
        << "Unique reference " << unused.first->asString()
        << " is never used.\n" << unused.second
      );

    // produce errors for any remaining moved paths
    for (auto moved : block.getMovedPaths())
      errors.push_back(LocatedError()
        << "Moved value " << moved.first->asString() << " is never replaced.\n"
        << moved.second
      );
  }

  /// @brief Main borrow checking function for expressions. Recursively
  /// traverses @p _e to perform a symbolic evaluation using access paths as
  /// the symbolic values.
  /// @return If @p _e has a reference or struct type, the access path for
  /// the expression is returned, otherwise nullptr.
  AccessPath* check(Exp* _e) {

    if (auto e = AddrOfExp::downcast(_e)) {
      Exp* ofExp = e->getOf();
      AccessPath* initAP = check(ofExp);
      AccessPath* ret = apm.getRoot(freshInternalVar());
      if (initAP != nullptr) apm.aliasDeref(ret, initAP);
      return ret;
    }
    else if (auto e = AscripExp::downcast(_e)) {
      return check(e->getAscriptee());
    }
    else if (auto e = AssignExp::downcast(_e)) {
      AccessPath* lhsAP = check(e->getLHS());
      AccessPath* rhsAP = check(e->getRHS());
      Type* rhsType = e->getRHS()->getType();
      auto lhsLooseExts = looseExtensionsOf(lhsAP, rhsType);
      auto rhsLooseExts = looseExtensionsOf(rhsAP, rhsType);
      for (AccessPath* ext : rhsLooseExts)
        bs->use(ext, e->getRHS()->getLocation());
      for (AccessPath* ext : lhsLooseExts)
        bs->unmove(ext, e->getLHS()->getLocation());
      return nullptr;
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
      for (auto owner : looseExtensionsOf(ret, e->getRefExp()->getType())) {
        if (LocationPair locs = bs->getUsedPaths().lookup(owner)) {
          errors.push_back(LocatedError()
            << "Unique reference " << owner->asString() << " created here:\n"
            << locs.fst << "is already used here:\n" << locs.snd
            << "so it cannot be borrowed later:\n"
            << e->getRefExp()->getLocation()
          );
        }
      }
      return ret;
    }
    else if (auto e = CallExp::downcast(_e)) {
      for (auto arg : e->getArguments()->asArrayRef()) {
        AccessPath* argAP = check(arg);
        for (AccessPath* ext : looseExtensionsOf(argAP, arg->getType())) {
          bs->use(ext, arg->getLocation());
        }
      }
      AccessPath* ret = apm.getRoot(freshInternalVar());
      for (auto looseExt : looseExtensionsOf(ret, e->getType())) {
        bs->intro(looseExt, e->getLocation());
      }
      return ret;
    }
    else if (auto e = ConstrExp::downcast(_e)) {
      AccessPath* ret = apm.getRoot(freshInternalVar());
      StructDecl* structDecl = ont.getType(e->getStruct()->asStringRef());
      llvm::ArrayRef<Exp*> args = e->getFields()->asArrayRef();
      auto fields = structDecl->getFields()->asArrayRef();
      assert(args.size() == fields.size());
      for (int i = 0; i < args.size(); ++i) {
        llvm::StringRef fieldName = fields[i].first->asStringRef();
        AccessPath* argAP = check(args[i]);
        if (argAP != nullptr) {
          apm.aliasProject(ret, fieldName, false, argAP);
        }
      }
      return ret;
    }
    else if (DecimalLit::downcast(_e)) {
      return nullptr;
    }
    else if (auto e = DerefExp::downcast(_e)) {
      AccessPath* ofAP = check(e->getOf());
      return apm.getDeref(ofAP);
    }
    else if (auto e = IfExp::downcast(_e)) {
      check(e->getCondExp());
      BorrowState* afterCond = bs;

      BorrowState afterThen = *afterCond;
      bs = &afterThen;
      AccessPath* thenAP = check(e->getThenExp());
      for (AccessPath* ap : looseExtensionsOf(thenAP, e->getType()))
        bs->use(ap, e->getThenExp()->getLocation());

      if (e->getElseExp() != nullptr) {
        BorrowState afterElse = *afterCond;
        bs = &afterElse;
        AccessPath* elseAP = check(e->getElseExp());
        for (AccessPath* ap : looseExtensionsOf(elseAP, e->getType()))
          bs->use(ap, e->getElseExp()->getLocation());

        afterThen.merge(afterElse, e->getLocation(), *afterCond);
        *afterCond = afterThen;
        bs = afterCond;
      } else {
        afterThen.merge(*afterCond, e->getLocation(), *afterCond);
        *afterCond = afterThen;
        bs = afterCond;
      }

      AccessPath* ret = apm.getRoot(freshInternalVar());
      for (AccessPath* ap : looseExtensionsOf(ret, e->getType()))
        bs->intro(ap, e->getLocation());
      return ret;
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
      for (AccessPath* loosePath : looseExtensionsOf(refAP, e->getType()))
        { bs->move(loosePath, e->getRefExp()->getLocation()); }
      AccessPath* ret = apm.getRoot(freshInternalVar());
      bs->intro(ret, e->getLocation());
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
      switch (e->getKind()) {
      case ProjectExp::DOT: return apm.getProject(baseAP, field, false);
      case ProjectExp::BRACKETS: return apm.getProject(baseAP, field, true);
      case ProjectExp::ARROW:
        return apm.getProject(apm.getDeref(baseAP), field, false);
      }
    }
    else if (StringLit::downcast(_e)) {
      return nullptr;
    }
    else if (auto e = UnopExp::downcast(_e)) {
      check(e->getInner());
      return nullptr;
    }
    else if (auto e = WhileExp::downcast(_e)) {
      check(e->getCond());
      BorrowState* const afterNoIters = bs;

      BorrowState afterOneIter = *afterNoIters;
      bs = &afterOneIter;
      check(e->getBody());
      check(e->getCond());

      afterOneIter.merge(*afterNoIters, e->getLocation(), *afterNoIters);
      bs = afterNoIters;
      return nullptr;
    }
    llvm_unreachable("BorrowChecker::check(Exp*) fallthrough");
    return nullptr;
  }

  /// @brief Append-only stack of borrow checker errors.
  llvm::SmallVector<LocatedError> errors;

private:

  AccessPathManager apm;

  TypeContext& tc;

  const Ontology& ont;

  BorrowState* bs;

  int nextInternalVar = 0;

  /// @brief Returns a fresh internal variable (like `$42`).
  std::string freshInternalVar()
    { return "$" + std::to_string(++nextInternalVar); }

  /// @brief Returns all _loose extensions_ of @p path. A loose extension is
  /// an AP that has @p path as a prefix and where all dereferences occurring
  /// after @p path dereference _unique_ refs.
  ///
  /// When a new value is introduced into a scope, the loose extensions of that
  /// value are precisely the additional access paths that must be used before
  /// the scope closes. Function parameters and function call returns are both
  /// examples of such value introductions.
  /// @param path If nullptr, an empty vector is returned.
  /// @param v The type of @p path
  llvm::SmallVector<AccessPath*> looseExtensionsOf(AccessPath* path, Type* t) {
    if (path == nullptr) return llvm::SmallVector<AccessPath*>();
    if (auto ty = Constraint::downcast(t)) {
      return {};
    }
    if (auto ty = NameType::downcast(t)) {
      llvm::SmallVector<AccessPath*> ret;
      StructDecl* structDecl = ont.getType(ty->asString);
      for (auto field : structDecl->getFields()->asArrayRef()) {
        llvm::StringRef fName = field.first->asStringRef();
        Type* fTy = tc.getTypeFromTypeExp(field.second);
        // TODO: a recursive type would cause a stack overflow.
        ret.append(looseExtensionsOf(apm.getProject(path, fName, false), fTy));
      }
      return ret;
    }
    if (auto ty = PrimitiveType::downcast(t)) {
      return {};
    }
    if (auto ty = RefType::downcast(t)) {
      if (ty->unique) {
        llvm::SmallVector<AccessPath*> ret = { path };
        ret.append(looseExtensionsOf(apm.getDeref(path), ty->inner));
        return ret;
      } else return {};
    }
    if (auto ty = TypeVar::downcast(t)) {
      llvm_unreachable("TypeVars are unsupported here.");
    }
    llvm_unreachable("BorrowChecker::looseExtensionsOf() unexpected case");
  }
};

#endif