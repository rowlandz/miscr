#ifndef BORROWCHECKER_HPP
#define BORROWCHECKER_HPP

#include <llvm/ADT/DenseMap.h>
#include "common/AST.hpp"
#include "common/LocatedError.hpp"
#include "common/TypeContext.hpp"
#include "borrowchecker/AccessPath.hpp"

/// @brief _Borrow Checker Identifier_. Represents an explicit identifier bound
/// in the source code or an internal identifier (e.g., `$42`).
class BCIdent {
  union { llvm::StringRef strVal; int intVal; } data;
  enum { EXPLICIT, INTERNAL } tag;
public:
  BCIdent(llvm::StringRef explicitIdent)
    : data({.strVal=explicitIdent}), tag(EXPLICIT) {}
  BCIdent(int internalIdent)
    : data({.intVal=internalIdent}), tag(INTERNAL) {}
  bool isExplicit() const { return tag == EXPLICIT; }
  bool isInternal() const { return tag == INTERNAL; }
};

/// @brief The borrow checker.
class BorrowChecker {
public:
  BorrowChecker(const TypeContext& tc) : tc(tc) {}

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
      llvm::StringRef paramName = param.first->asStringRef();
      // accessPaths[paramName] = apm.getRoot(paramName);
      if (auto paramType = RefTypeExp::downcast(param.second)) {
        if (paramType->isOwned()) {
          AccessPath* paramRoot = apm.getRoot(paramName);
          ownerCreations[paramRoot] = param.first->getLocation();
        }
      }
    }

    AccessPath* retAP = check(funcDecl->getBody());
    if (isOwnedRef(funcDecl->getBody()->getTVar())) {
      // TODO: make helper function to narrow the location for block exps
      if (auto block = BlockExp::downcast(funcDecl->getBody())) {
        use(retAP, block->getStatements()->asArrayRef().back()->getLocation());
      } else {
        use(retAP, funcDecl->getBody()->getLocation());
      }
    }

    // If any owned references weren't used, produce an error.
    for (auto owner : ownerCreations) {
      Location use = ownerUses.lookup(owner.first);
      if (!use.exists()) {
        LocatedError err;
        err.append("Owned reference is never used.\n");
        err.append(owner.second);
        errors.push_back(err);
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
      // no binary operator should operate over owned references
      assert(!isOwnedRef(e->getLHS()->getTVar()));
      assert(!isOwnedRef(e->getRHS()->getTVar()));
      check(e->getLHS());
      check(e->getRHS());
      return nullptr;
    }
    else if (auto e = BlockExp::downcast(_e)) {
      if (e->getStatements()->asArrayRef().empty()) return nullptr;
      llvm::ArrayRef<Exp*> stmts = e->getStatements()->asArrayRef().drop_back();
      Exp* returnExp = e->getStatements()->asArrayRef().back();
      for (auto stmt : stmts) {
        check(stmt);
        if (isOwnedRef(stmt->getTVar())) {
          LocatedError err;
          err.append("Owned reference is discarded.\n");
          err.append(stmt->getLocation());
          errors.push_back(err);
        }
      }
      return check(returnExp);
    }
    else if (BoolLit::downcast(_e)) {
      return nullptr;
    }
    else if (auto e = BorrowExp::downcast(_e)) {
      AccessPath* ret = check(e->getRefExp());
      borrow(ret, e->getRefExp()->getLocation());
      return ret;
    }
    else if (auto e = CallExp::downcast(_e)) {
      if (e->isConstr())
        llvm_unreachable("Constructor calls not supported yet by borrow checker.");
      for (auto arg : e->getArguments()->asArrayRef()) {
        AccessPath* argAP = check(arg);
        if (isOwnedRef(arg->getTVar()))
          use(argAP, arg->getLocation());
      }
      AccessPath* ret = apm.getRoot(freshInternalVar());
      if (isOwnedRef(e->getTVar())) {
        createOwner(ret, e->getLocation());
      }
      return ret;
    }
    else if (DecimalLit::downcast(_e)) {
      return nullptr;
    }
    else if (IntLit::downcast(_e)) {
      return nullptr;
    }
    else if (auto e = NameExp::downcast(_e)) {
      llvm::StringRef name = e->getName()->asStringRef();
      return apm.getRoot(name);
    }
    else if (auto e = LetExp::downcast(_e)) {
      AccessPath* defAP = check(e->getDefinition());
      if (isOwnedRef(e->getDefinition()->getTVar())) {
        use(defAP, e->getDefinition()->getLocation());
        AccessPath* ret = apm.getRoot(e->getBoundIdent()->asStringRef());
        createOwner(ret, e->getBoundIdent()->getLocation());
      }
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
  
  AccessPathManager apm;

  const TypeContext& tc;

  /// @brief Maps local owners to their source code creation locations.
  llvm::DenseMap<AccessPath*, Location> ownerCreations;

  /// @brief Maps local owners to the locations where they are used.
  /// The keyset of this map must be a subset of ownerCreations.
  llvm::DenseMap<AccessPath*, Location> ownerUses;

  int nextInternalVar = 0;

  /// @brief Returns a fresh internal variable (like `$42`).
  std::string freshInternalVar()
    { return std::string("$") + std::to_string(++nextInternalVar); }

  /// @brief Returns true iff type variable @p v resolves to an oref type. 
  bool isOwnedRef(TVar v) {
    Type blah = tc.resolve(v).second;
    return blah.getID() == Type::ID::OREF;
  }

  /// @brief Marks @p ap as an owner, created at @p loc.
  void createOwner(AccessPath* ap, Location loc) {
    ownerCreations[ap] = loc;
  }

  /// @brief Checks that @p ap has not been used yet and can be borrowed. It is
  /// safe to pass `nullptr` to this function.
  /// @param loc Location where @p ap is borrowed.
  void borrow(AccessPath* ap, Location loc) {
    if (ap == nullptr) return;
    Location useLoc = ownerUses.lookup(ap);
    if (useLoc.exists()) {
      LocatedError err;
      err.append("Owned reference ");
      err.append(ap->asString());
      err.append(" has already been used.\n");
      err.append(loc);
      err.append("Use occurs here:\n");
      err.append(useLoc);
      errors.push_back(err);
    }
  }

  /// @brief Marks @p ap as used. @p ap should be the access path of an owned
  /// reference. Pushes an error if @p ap has already been used. If @p ap is
  /// `nullptr`, then does nothing.
  /// @param loc the location where the use occurs
  void use(AccessPath* ap, Location loc) {
    if (ap == nullptr) return;
    Location useLoc = ownerUses.lookup(ap);
    if (useLoc.exists()) {
      LocatedError err;
      err.append("Owned reference ");
      err.append(ap->asString());
      err.append(" has already been used.\n");
      err.append(loc);
      err.append("Use occurs here:\n");
      err.append(useLoc);
      errors.push_back(err);
    } else {
      ownerUses[ap] = loc;
    }
  }
};

/*

Owned references can be "used" by these expression forms:

BinopExp (not really, but maybe if there's a binop that acts on refs)
IfExp
  - The two branches must agree on which orefs are used inside it.
BlockExp
  - Any statement other than the last cannot be an oref.
CallExp
AscripExp
  - Can consume an oref as long as it produces the same oref.
LetExp
  - Binding an oref to a ident counts as a use.
ReturnExp
StoreExp
  - oref on RHS counts as a "use"
  - oref is not allowed on LHS.
RefExp
MoveExp
  - kind of. The owned reference must be accessed through a bref.


Example translating String::append into BCIR

func append(s1: &String, s2: &String): unit = {
  let s1len: i64 = s1[.len]!;
  let newLen: i64 = s1len + s2[.len]!;
  let newPtr: #i8 = C::realloc(move s1[.ptr], newLen + 1);
  C::strcpy((borrow newPtr)[s1len], borrow s2[.ptr]!);
  s1[.ptr] := newPtr;
  s1[.len] := newLen;
  {}
};

// start with no owners
intro s1      disjoint           // s1 is an access path (disjoint from prev)
intro s2      disjoint
intro s1len   s1 [String.len] !  // s1len is an AP eq to s1[String.len]!
intro $0      s2 [String.len] !
intro newLen  disjoint
intro $1      s1 [String.ptr]
mov   $1                         // invalidate AP $1! until unmov $1
intro $2      disjoint owner     // the result of the "move" expr.
use   $2                         // immediately use $2 as a function arg
intro $3      disjoint owner     // the result of C::realloc
use   $3                         // use $3 in RHS of let-expr
intro newPtr  disjoint owner     // introduction via let-expr
intro $4      newPtr             // "borrow" creates a new ident without a "use"
intro $5      $4 [s1len]
intro $6      s2 [String.ptr] !  // the deref implicitly borrows the owned ptr.
intro $7      s1 [String.ptr]
use   newPtr
unmov $7                         // $7 == $1, so $1 has been unmoved now

BC Interpretation State:
  Access path bindings
  for each owner identifier:
    where it is created
    where it is used (if anywhere)
  for each moved access path:
    where the move occurred




An identifier can "own" a reference.
An owner must be "used" exactly once before going out of scope.

An access path can be "invalidated" by a move expression.
An invalidated access path can be "revalidated" by a store expression.



*/

#endif