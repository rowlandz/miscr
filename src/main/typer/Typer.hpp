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

  Typer() : cataloger(&ont), unifier(&ont) {}

  const TypeContext* getTypeContext() const { return unifier.getTypeContext(); }

  void typeExp(Exp* _exp) {
    unifier.unifyExp(_exp);
  }

  void typeDecl(Decl* _decl) {
    cataloger.catalog(std::string("global"), _decl);
    unifier.unifyDecl(_decl);
  }

  void typeDeclList(DeclList* _declList) {
    cataloger.catalogDeclList(std::string("global"), _declList);
    unifier.unifyDeclList(_declList);
  }

};

#endif