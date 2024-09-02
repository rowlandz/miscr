#ifndef TYPER_TYPER
#define TYPER_TYPER

#include "typer/Unifier.hpp"
#include "typer/Resolver.hpp"

namespace Typer {
  
  void typeExp(unsigned int _n, NodeManager* m) {
    Unifier(m).tyExp(_n);
    Resolver(m).resolveExp(_n);
  }

  void typeFuncOrProc(unsigned int _n, NodeManager* m) {
    Unifier(m).tyFuncOrProc(_n);
    Resolver(m).resolveDecl(_n);
  }

}

#endif