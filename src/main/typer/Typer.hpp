#ifndef TYPER_TYPER
#define TYPER_TYPER

#include "typer/Cataloger.hpp"
#include "typer/Unifier.hpp"
#include "typer/Resolver.hpp"

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
  Resolver resolver;

  Typer(NodeManager* m) : cataloger(m, &ont), unifier(m, &ont), resolver(m) {}

  void typeExp(unsigned int _n) {
    unifier.tyExp(_n);
    resolver.resolveExp(_n);
  }

  void typeDecl(unsigned int _n) {
    cataloger.catalog(std::string("global"), _n);
    unifier.tyDecl(_n);
    resolver.resolveDecl(_n);
  }

};

#endif