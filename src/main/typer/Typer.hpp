#ifndef TYPER_TYPER
#define TYPER_TYPER

#include "typer/Cataloger.hpp"
#include "typer/Unifier.hpp"
#include "typer/Resolver.hpp"

class Typer {
public:
  Ontology ont;
  Cataloger cataloger;
  Unifier unifier;
  Resolver resolver;

  Typer(NodeManager* m) : cataloger(m), unifier(m, &ont), resolver(m) {}

  void typeExp(unsigned int _n) {
    unifier.tyExp(_n);
    resolver.resolveExp(_n);
  }

  void typeFuncOrProc(unsigned int _n) {
    unifier.tyFuncOrProc(_n);
    resolver.resolveDecl(_n);
  }

  void typeDecl(unsigned int _n) {
    cataloger.catalog(std::string("global"), _n);
    unifier.tyDecl(_n);
    resolver.resolveDecl(_n);
  }

};

#endif