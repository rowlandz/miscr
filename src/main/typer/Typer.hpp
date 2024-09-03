#ifndef TYPER_TYPER
#define TYPER_TYPER

#include "typer/Cataloger.hpp"
#include "typer/Unifier.hpp"
#include "typer/Resolver.hpp"

namespace Typer {
  
  void typeExp(unsigned int _n, NodeManager* m) {
    Ontology ont;
    Unifier(m, &ont).tyExp(_n);
    Resolver(m).resolveExp(_n);
  }

  void typeFuncOrProc(unsigned int _n, NodeManager* m) {
    Ontology ont;
    Unifier(m, &ont).tyFuncOrProc(_n);
    Resolver(m).resolveDecl(_n);
  }

  void typeDecl(unsigned int _n, NodeManager* m) {
    Cataloger c(m);
    c.catalog(std::string("global"), _n);
    Unifier(m, &c.ont).tyDecl(_n);
    Resolver(m).resolveDecl(_n);
  }

}

#endif