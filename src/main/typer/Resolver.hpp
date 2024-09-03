#ifndef RESOLVER_HPP
#define RESOLVER_HPP

#include "common/NodeManager.hpp"

/**
 * Second of two type checking phases. Replaces all type variables with
 * the types they resolve to.
 */
class Resolver {
public:
  NodeManager* m;

  Resolver(NodeManager* m) {
    this->m = m;
  }

  // TODO: this is copied from Unifier.hpp
  /** Resolves a type until a non-TYPEVAR is reached or a terminal TYPEVAR
   * is reached. `_ty` should refer to a type node. */
  unsigned int resolve(unsigned int _ty) {
    Node ty = m->get(_ty);
    while (ty.ty == NodeTy::TYPEVAR && ty.n1 != NN) {
      _ty = ty.n1;
      ty = m->get(_ty);
    }
    return _ty;
  }

  void resolveExp(unsigned int _n) {
    Node n = m->get(_n);
    n.n1 = resolve(n.n1);
    m->set(_n, n);
    
    switch(n.ty) {
      case NodeTy::ADD:
      case NodeTy::SUB:
      case NodeTy::MUL:
      case NodeTy::DIV:
      case NodeTy::EQ:
      case NodeTy::NE:
        resolveExp(n.n2);
        resolveExp(n.n3);
        break;
      case NodeTy::BLOCK:
        resolveStmtList(n.n2);
        break;
      case NodeTy::CALL:
        resolveExpList(n.n2);
        break;
      case NodeTy::IF:
        resolveExp(n.n2);
        resolveExp(n.n3);
        resolveExp(n.extra.nodes.n4);
        break;
      case NodeTy::EQIDENT:
      case NodeTy::FALSE:
      case NodeTy::TRUE:
      case NodeTy::LIT_DEC:
      case NodeTy::LIT_INT:
      case NodeTy::LIT_STRING:
        break;
      default:
        printf("Unifier case not supported yet!\n");
        exit(1);
    }
  }

  void resolveStmtList(unsigned int _n) {
    Node stmtList = m->get(_n);
    while (stmtList.ty == NodeTy::STMTLIST_CONS) {
      resolveStmt(stmtList.n1);
      stmtList = m->get(stmtList.n2);
    }
  }

  void resolveExpList(unsigned int _n) {
    Node expList = m->get(_n);
    while (expList.ty == NodeTy::EXPLIST_CONS) {
      resolveExp(expList.n1);
      expList = m->get(expList.n2);
    }
  }

  void resolveStmt(unsigned int _n) {
    Node n = m->get(_n);

    if (n.ty == NodeTy::LET) {
      resolveExp(n.n2);
    }

    else if (isExpNodeTy(n.ty)) {
      resolveExp(_n);
    }

    else {
      printf("Unsupported case in Resolver::resolveExp!\n");
      exit(1);
    }
  }

  void resolveDecl(unsigned int _n) {
    Node n = m->get(_n);

    if (n.ty == NodeTy::FUNC || n.ty == NodeTy::PROC) {
      resolveExp(n.extra.nodes.n4);
    }

    else if (n.ty == NodeTy::MODULE || n.ty == NodeTy::NAMESPACE) {
      for (Node declList = m->get(n.n2); declList.ty == NodeTy::DECLLIST_CONS; declList = m->get(declList.n2)) {
        resolveDecl(declList.n1);
      }
    }

    else if (n.ty == NodeTy::EXTERN_FUNC || n.ty == NodeTy::EXTERN_PROC) {
      // nothing to be done
    }

    else {
      printf("Unexpected decl in Resolver!\n");
      exit(1);
    }
  }
};

#endif