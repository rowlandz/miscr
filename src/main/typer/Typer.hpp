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
  std::vector<LocatedError> errors;

  Typer() : cataloger(ont, errors), unifier(ont, errors) {}

  TypeContext& getTypeContext() { return unifier.getTypeContext(); }

  void typeExp(Exp* _exp) {
    unifier.unifyExp(_exp);
  }

  void typeDecl(Decl* _decl) {
    cataloger.run(_decl);
    if (!errors.empty()) return;
    unifier.unifyDecl(_decl);
  }

  void typeDeclList(DeclList* _declList) {
    cataloger.run(_declList);
    if (!errors.empty()) return;
    unifier.unifyDeclList(_declList);
  }

};

#endif