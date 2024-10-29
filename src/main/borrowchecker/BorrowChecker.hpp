#ifndef BORROWCHECKER_HPP
#define BORROWCHECKER_HPP

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

  /// @brief Main borrow checking function for a function. The `accessPaths`,
  /// `ownerCreations`, and `ownerUses` are reset.
  void checkFunctionDecl(FunctionDecl* funcDecl) {
    if (funcDecl->getBody() == nullptr) return;
    accessPaths.clear();
    ownerCreations.clear();
    ownerUses.clear();
    for (auto param : funcDecl->getParameters()->asArrayRef()) {
      llvm::StringRef paramName = param.first->asStringRef();
      accessPaths[paramName] = apm.getRoot(paramName);
      if (auto paramType = RefTypeExp::downcast(param.second)) {
        if (paramType->isOwned()) {
          ownerCreations[paramName] = param.first->getLocation();
        }
      }
    }

    check(funcDecl->getBody());

    // If any owned references weren't used, produce an error.
    for (llvm::StringRef owner : ownerCreations.keys()) {
      Location use = ownerUses.lookup(owner);
      if (!use.exists()) {
        LocatedError err;
        err.append("Owned reference is never used.\n");
        err.append(ownerCreations[owner]);
        errors.push_back(err);
      }
    }
  }

  /// @brief Main borrow checking function for expressions.
  /// @param borrowed True iff this expression occurs in a position that
  /// dictates it is borrowed (such as in a `borrow` or deref expression).
  void check(Exp* _e, bool borrowed = false) {

    if (auto e = AscripExp::downcast(_e)) {
      check(e->getAscriptee());
    }
    else if (auto e = BinopExp::downcast(_e)) {
      // no binary operator should operate over owned references
      assert(!isOwnedRef(e->getLHS()->getTVar()));
      assert(!isOwnedRef(e->getRHS()->getTVar()));
      check(e->getLHS());
      check(e->getRHS());
    }
    else if (auto e = BlockExp::downcast(_e)) {
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
      check(returnExp);
    }
    else if (BoolLit::downcast(_e)) {

    }
    else if (auto e = BorrowExp::downcast(_e)) {
      check(e->getRefExp(), true);
    }
    else if (auto e = CallExp::downcast(_e)) {
      if (e->isConstr())
        llvm_unreachable("Constructor calls not supported yet by borrow checker.");
      for (auto arg : e->getArguments()->asArrayRef()) check(arg);
    }
    else if (DecimalLit::downcast(_e)) {

    }
    else if (IntLit::downcast(_e)) {

    }
    else if (auto e = NameExp::downcast(_e)) {
      llvm::StringRef name = e->getName()->asStringRef();
      if (isOwnedRef(e->getTVar())) {
        Location use = ownerUses.lookup(name);
        if (use.exists()) {
          LocatedError err;
          err.append("Owned reference ");
          err.append(name);
          err.append(" has already been used.\n");
          err.append(e->getLocation());
          err.append("Use occurs here:\n");
          err.append(use);
          errors.push_back(err);
        } else if (!borrowed) {
          ownerUses[name] = e->getLocation();
        }
      }
    }
    else if (auto e = LetExp::downcast(_e)) {
      check(e->getDefinition());
      if (isOwnedRef(e->getDefinition()->getTVar())) {
        ownerCreations[e->getBoundIdent()->asStringRef()] =
          e->getBoundIdent()->getLocation();
      }
    }
    else if (StringLit::downcast(_e)) {

    }
    else llvm_unreachable("BorrowChecker::check(Exp*): unexpected syntax");

  }

  /// @brief Append-only stack of borrow checker errors.
  llvm::SmallVector<LocatedError> errors;

private:
  
  AccessPathManager apm;

  const TypeContext& tc;

  /// @brief Maps identifiers to access paths they are bound to.
  llvm::StringMap<AccessPath*> accessPaths;

  /// @brief Maps owner identifiers to the locations where they are created.
  llvm::StringMap<Location> ownerCreations;

  /// @brief Maps owner identifiers to the locations where they are used.
  /// The keyset of this map must be a subset of ownerCreations.
  llvm::StringMap<Location> ownerUses;

  /// @brief Returns true iff type variable @p v resolves to an oref type. 
  bool isOwnedRef(TVar v) {
    Type blah = tc.resolve(v).second;
    return blah.getID() == Type::ID::OREF;
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