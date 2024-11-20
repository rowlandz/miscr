#ifndef TYPER_RESOLVER
#define TYPER_RESOLVER

#include <llvm/ADT/DenseMap.h>
#include "common/AST.hpp"
#include "common/TypeContext.hpp"

/// @brief Fourth of four type checking phases. Scrubs AST of type variables.
class Resolver {

  const llvm::DenseMap<TypeVar*, TypeVar*>& tvarEquiv;

  const llvm::DenseMap<TypeVar*, Type*>& tvarBindings;

  TypeContext& tc;

  // TODO: code duplication with Unifier.
  /// @brief Returns the representative value of the equivalence class of @p v
  /// (i.e., the `find` operation of union-find).
  TypeVar* find(TypeVar* v) {
    while (auto v1 = tvarEquiv.lookup(v)) v = v1;
    return v;
  }

public:
  Resolver(llvm::DenseMap<TypeVar*, TypeVar*>& tvarEquiv,
           llvm::DenseMap<TypeVar*, Type*>& tvarBindings,
           TypeContext& tc)
    : tvarEquiv(tvarEquiv), tvarBindings(tvarBindings), tc(tc) {}

  Resolver(const Resolver&) = delete;

  /// @brief Recursively resolves all type variables in this AST element.
  void resolveAST(AST* ast) {
    assert(ast != nullptr && "Shouldn't resolve nullptr");
    if (Exp* e = Exp::downcast(ast)) {
      e->setType(resolveType(e->getType()));
      for (AST* child : ast->getASTChildren()) resolveAST(child);
    }
    else if (DataDecl::downcast(ast))
      { /* do nothing */ } 
    else if (FunctionDecl* func = FunctionDecl::downcast(ast))
      { if (func->getBody() != nullptr) resolveAST(func->getBody()); }
    else { for (AST* child : ast->getASTChildren()) resolveAST(child); }
  }

  /// @brief Removes all type variables from @p ty.
  Type* resolveType(Type* ty) {
    if (auto v = TypeVar::downcast(ty)) {
      TypeVar* w = find(v);
      Type* t = tvarBindings.lookup(w);
      assert(t != nullptr);
      assert(TypeVar::downcast(t) == nullptr);
      return t;
    }
    if (auto refTy = RefType::downcast(ty)) {
      return tc.getRefType(resolveType(refTy->inner), refTy->isOwned);
    }
    if (Constraint::downcast(ty)) return ty;
    if (NameType::downcast(ty)) return ty;
    if (PrimitiveType::downcast(ty)) return ty;
    llvm_unreachable("Resolver::resolveType() unexpected case");
  }

};

#endif