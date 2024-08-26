#ifndef TYPER_TYPER
#define TYPER_TYPER

#include <string>
#include <map>
#include "common/NodeManager.hpp"

/**
 * Annotates expressions with types.
 * TODO: Do we want to maintain the tree structure or is it okay to break it here?
 * Currently I'm assuming it's okay to break it.
 */
class Typer {
public:

  NodeManager* m;
 
  /** Maps local variable names to type nodes. */
  std::map<std::string, unsigned int> localIdents;

  /** Accumulates type checking errors. */
  std::vector<std::string> errors;

  // Nodes are initialized for common types to avoid making multiple nodes.
  unsigned int ty_bool;
  unsigned int ty_i8;
  unsigned int ty_i32;
  unsigned int tyc_numeric;

  Typer(NodeManager* m) {
    this->m = m;
    ty_bool = m->add({ { 0, 0, 0 }, NN, NN, NN, NOEXTRA, NodeTy::BOOL });
    ty_i8 = m->add({ { 0, 0, 0 }, NN, NN, NN, NOEXTRA, NodeTy::i8 });
    ty_i32 = m->add({ { 0, 0, 0 }, NN, NN, NN, NOEXTRA, NodeTy::i32 });
    tyc_numeric = m->add({ { 0, 0, 0 }, NN, NN, NN, NOEXTRA, NodeTy::NUMERIC });
  }

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

  /** Resolves a type, except the second-to-last type is returned (the last
   * TYPEVAR) instead of the last type. */
  unsigned int almostResolve(unsigned int _tyVar) {
    Node tyVar = m->get(_tyVar);
    while (tyVar.n1 != NN) {
      Node ty2 = m->get(tyVar.n1);
      if (ty2.ty != NodeTy::TYPEVAR) return _tyVar;
      _tyVar = tyVar.n1;
      tyVar = ty2;
    }
    return _tyVar;
  }

  /** Binds `_tyVar` to `_toTy`. */
  void bind(unsigned int _tyVar, unsigned int _toTy) {
    Node tyVar = m->get(_tyVar);
    // can remove the following if block when we're sure this is safe.
    if (tyVar.ty != NodeTy::TYPEVAR) { printf("Expected typevar here!!! Got %hu\n", tyVar.ty); exit(1); }
    tyVar.n1 = _toTy;
    m->set(_tyVar, tyVar);
  }

  /** Enforces the constraint that the two types resolve to the same type.
   * The first type must be a type variable.
   * Returns true if unification succeeded. */
  bool unify(unsigned int _tyVar1, unsigned int _ty2) {
    unsigned int _rtyVar1 = almostResolve(_tyVar1);
    unsigned int _rty1 = resolve(_rtyVar1);
    unsigned int _rty2 = resolve(_ty2);
    Node rty1 = m->get(_rty1);
    Node rty2 = m->get(_rty2);

    if (rty1.ty == rty2.ty) {
      if (rty1.ty == NodeTy::i8 || rty1.ty == NodeTy::i32 || rty1.ty == NodeTy::BOOL || rty1.ty == NodeTy::NUMERIC) {
        // do nothing
      } else if (rty1.ty == NodeTy::TYPEVAR) {
        bind(_rtyVar1, _rty2);
      } else {
        printf("Fatal unification error: node is not a type\n");
        exit(1);
      }
    } else if (rty1.ty == NodeTy::TYPEVAR) {
      bind(_rtyVar1, _rty2);
    } else if (rty2.ty == NodeTy::TYPEVAR) {
      bind(_rty2, _rty1);
    } else if (rty1.ty == NodeTy::NUMERIC) {
      switch (rty2.ty) {
        case NodeTy::i8:
        case NodeTy::i32:
          bind(_rtyVar1, _rty2);
          break;
        default:
          errors.push_back(unificationError(_rty1, _rty2));
          return false;
      }
    } else if (rty2.ty == NodeTy::NUMERIC) {
      switch (rty1.ty) {
        case NodeTy::i8:
        case NodeTy::i32:
          if (m->get(_ty2).ty == NodeTy::TYPEVAR) {
            bind(almostResolve(_ty2), _rty1);
          }
          break;
        default:
          errors.push_back(unificationError(_rty1, _rty2));
          return false;
      }
    } else {
      errors.push_back(unificationError(_rty1, _rty2));
      return false;
    }
    return true;
  }

  /** Typechecks an expression node at location `_n` in `m`.
   * Returns The type node `m->get(_n).n1` of the expression for convenience. */
  unsigned int tyExp(unsigned int _n) {
    Node n = m->get(_n);

    if (n.ty == NodeTy::ADD || n.ty == NodeTy::SUB || n.ty == NodeTy::MUL || n.ty == NodeTy::DIV) {
      unsigned int tyn2 = tyExp(n.n2);
      unsigned int tyn3 = tyExp(n.n3);
      unify(tyn2, tyc_numeric);
      unify(tyn3, tyn2);
      bind(n.n1, tyn2);
    }

    else if (n.ty == NodeTy::EIDENT) {
      std::string identStr(n.extra.ptr, n.loc.sz);
      bind(n.n1, localIdents[identStr]);
    }

    else if (n.ty == NodeTy::EQ || n.ty == NodeTy::NE) {
      unsigned int tyn2 = tyExp(n.n2);
      unsigned int tyn3 = tyExp(n.n3);
      unify(tyn3, tyn2);
      bind(n.n1, ty_bool);
    }

    else if (n.ty == NodeTy::FALSE || n.ty == NodeTy::TRUE) {
      bind(n.n1, ty_bool);
    }

    else if (n.ty == NodeTy::IF) {
      unsigned int tyn2 = tyExp(n.n2);
      unsigned int tyn3 = tyExp(n.n3);
      unsigned int tyn4 = tyExp(n.extra.nodes.n4);
      unify(tyn2, ty_bool);
      unify(tyn4, tyn3);
      unify(n.n1, tyn3);
    }

    else if (n.ty == NodeTy::LIT_INT) {
      bind(n.n1, tyc_numeric);
    }

    else {
      printf("Typer case not supported yet!\n");
      exit(1);
    }

    return n.n1;
  }

  /** Same as `tyExp` but the return value is `void`. */
  void voidTyExp(unsigned int _n) {
    tyExp(_n);
  }

  void addParamsToLocalIdents(unsigned int _paramList) {
    Node paramList = m->get(_paramList);
    while (paramList.ty == NodeTy::PARAMLIST_CONS) {
      Node paramName = m->get(paramList.n1);
      std::string paramStr(paramName.extra.ptr, paramName.loc.sz);
      localIdents[paramStr] = paramList.n2;
      paramList = m->get(paramList.n3);
    }
  }

  void tyFuncOrProc(unsigned int _n) {
    Node n = m->get(_n);
    localIdents.clear();
    addParamsToLocalIdents(n.n2);
    unsigned int bodyType = tyExp(n.extra.nodes.n4);
    unify(bodyType, n.n3);
  }

  std::string tyNodeToString(unsigned int _ty) {
    Node ty = m->get(_ty);
    switch (ty.ty) {
      case NodeTy::BOOL: return std::string("bool");
      case NodeTy::i8: return std::string("i8");
      case NodeTy::i32: return std::string("i32");
      case NodeTy::NUMERIC: return std::string("numeric");
      default: printf("Not a type!\n"); exit(1);
    }
  }

  std::string unificationError(unsigned int _ty1, unsigned int _ty2) {
    return std::string("Cannot unify ") + tyNodeToString(_ty1) + " with " + tyNodeToString(_ty2);
  }

};

#endif
