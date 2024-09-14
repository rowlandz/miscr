#ifndef TYPER_TYPER
#define TYPER_TYPER

#include "typer/Cataloger.hpp"
#include "typer/Unifier.hpp"

/**
 * The type checker and type inference engine.
 * Typing is divided into three sequential phases:
 *   1. Cataloger -- Builds a map of decl names to their definitions
 *   2. Unifier   -- Hindley-Milner type unification procedure
 *   3. Resolver  -- Replaces type variables with their resolutions
 */
class Typer {
public:
  Ontology ont;
  Cataloger cataloger;
  Unifier unifier;

  Typer(ASTContext* m) : cataloger(m, &ont), unifier(m, &ont) {}

  const TypeContext* getTypeContext() const { return unifier.getTypeContext(); }

  void typeExp(Addr<Exp> _exp) {
    unifier.unifyExp(_exp);
  }

  void typeDecl(Addr<Decl> _decl) {
    cataloger.catalog(std::string("global"), _decl);
    unifier.unifyDecl(_decl);
  }

  void typeDeclList(unsigned int _declList) {
    // cataloger.catalogDeclList(std::string("global"), _declList);
    // unifier.tyDeclList(_declList);
    assert(false && "unimplemented");
  }

};

#endif