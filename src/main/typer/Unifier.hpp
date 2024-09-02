#ifndef TYPER_UNIFIER
#define TYPER_UNIFIER

#include <string>
#include <map>
#include "common/ScopeStack.hpp"
#include "common/NodeManager.hpp"

/** First of two type checking phases.
 * Annotates expressions with types.
 * TODO: Do we want to maintain the tree structure or is it okay to break it here?
 * Currently I'm assuming it's okay to break it.
 */
class Unifier {
public:

  NodeManager* m;
 
  /** Maps local variable names to type nodes. */
  ScopeStack<unsigned int> localVarTypes;

  /** Accumulates type checking errors. */
  std::vector<std::string> errors;

  // Nodes are initialized for common types to avoid making multiple nodes.
  unsigned int ty_unit;
  unsigned int ty_bool;
  unsigned int ty_i8;
  unsigned int ty_i32;
  unsigned int ty_string;
  unsigned int tyc_numeric;
  unsigned int tyc_decimal;

  Unifier(NodeManager* m) {
    this->m = m;
    ty_unit = m->add({ { 0, 0, 0 }, NN, NN, NN, NOEXTRA, NodeTy::UNIT });
    ty_bool = m->add({ { 0, 0, 0 }, NN, NN, NN, NOEXTRA, NodeTy::BOOL });
    ty_i8 = m->add({ { 0, 0, 0 }, NN, NN, NN, NOEXTRA, NodeTy::i8 });
    ty_i32 = m->add({ { 0, 0, 0 }, NN, NN, NN, NOEXTRA, NodeTy::i32 });
    ty_string = m->add({ { 0, 0, 0 }, NN, NN, NN, NOEXTRA, NodeTy::STRING });
    tyc_numeric = m->add({ { 0, 0, 0 }, NN, NN, NN, NOEXTRA, NodeTy::NUMERIC });
    tyc_decimal = m->add({ { 0, 0, 0 }, NN, NN, NN, NOEXTRA, NodeTy::DECIMAL });
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
  void unify(unsigned int _tyVar1, unsigned int _ty2) {
    unsigned int _rtyVar1 = almostResolve(_tyVar1);
    unsigned int _rty1 = resolve(_rtyVar1);
    unsigned int _rty2 = resolve(_ty2);
    Node rty1 = m->get(_rty1);
    Node rty2 = m->get(_rty2);

    if (rty1.ty == rty2.ty) {
      if (rty1.ty == NodeTy::f32 || rty1.ty == NodeTy::f64 || rty1.ty == NodeTy::i8
      || rty1.ty == NodeTy::i32 || rty1.ty == NodeTy::BOOL || rty1.ty == NodeTy::STRING
      || rty1.ty == NodeTy::UNIT
      || rty1.ty == NodeTy::NUMERIC || rty1.ty == NodeTy::DECIMAL) {
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
        case NodeTy::f32:
        case NodeTy::f64:
        case NodeTy::i8:
        case NodeTy::i32:
        case NodeTy::DECIMAL:
          bind(_rtyVar1, _rty2);
          break;
        default:
          errors.push_back(unificationError(_rty1, _rty2));
      }
    } else if (rty2.ty == NodeTy::NUMERIC) {
      switch (rty1.ty) {
        case NodeTy::f32:
        case NodeTy::f64:
        case NodeTy::i8:
        case NodeTy::i32:
        case NodeTy::DECIMAL:
          if (m->get(_ty2).ty == NodeTy::TYPEVAR) {
            bind(almostResolve(_ty2), _rty1);
          }
          break;
        default:
          errors.push_back(unificationError(_rty1, _rty2));
      }
    } else if (rty1.ty == NodeTy::DECIMAL) {
      switch (rty2.ty) {
        case NodeTy::f32:
        case NodeTy::f64:
          bind(_rtyVar1, _rty2);
          break;
        default:
          errors.push_back(unificationError(_rty1, _rty2));
      }
    } else if (rty2.ty == NodeTy::DECIMAL) {
      switch (rty1.ty) {
        case NodeTy::f32:
        case NodeTy::f64:
          if (m->get(_ty2).ty == NodeTy::TYPEVAR) {
            bind(almostResolve(_ty2), _rty1);
          }
          break;
        default:
          errors.push_back(unificationError(_rty1, _rty2));
      }
    }
    
    else {
      errors.push_back(unificationError(_rty1, _rty2));
    }
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

    else if (n.ty == NodeTy::BLOCK) {
      localVarTypes.push();
      Node stmtList = m->get(n.n2);
      unsigned int lastStmtTy = ty_unit;
      while (stmtList.ty == NodeTy::STMTLIST_CONS) {
        lastStmtTy = tyStmt(stmtList.n1);
        stmtList = m->get(stmtList.n2);
      }
      bind(n.n1, lastStmtTy);
      localVarTypes.pop();
    }

    // TODO: this is broken
    // else if (n.ty == NodeTy::EQIDENT) {
    //   std::string identStr(n.extra.ptr, n.loc.sz);
    //   unsigned int varTyNode = localVarTypes.getOrElse(identStr, NN);
    //   if (varTyNode != NN) {
    //     bind(n.n1, varTyNode);
    //   } else {
    //     errors.push_back(varNotFoundError(identStr));
    //   }
    // }

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

    else if (n.ty == NodeTy::LIT_DEC) {
      bind(n.n1, tyc_decimal);
    }

    else if (n.ty == NodeTy::LIT_INT) {
      bind(n.n1, tyc_numeric);
    }

    else if (n.ty == NodeTy::LIT_STRING) {
      bind(n.n1, ty_string);
    }

    else {
      printf("Unifier case not supported yet!\n");
      exit(1);
    }

    return n.n1;
  }

  /** Typechecks a statement pointed by by `_n`. Any variables introduced via binding
   * are added to the topmost scope frame. Returns the type of the statement. */
  unsigned int tyStmt(unsigned int _n) {
    Node n = m->get(_n);

    if (n.ty == NodeTy::LET) {
      Node n1 = m->get(n.n1);
      unsigned int tyn2 = tyExp(n.n2);
      std::string identStr(n1.extra.ptr, n1.loc.sz);
      localVarTypes.add(identStr, tyn2);
      return ty_unit;
    }

    else if (isExpNodeTy(n.ty)) {
      return tyExp(_n);
    }

    else {
      printf("Unifier case not supported yet!!\n");
      exit(1);
    }
  }

  /** Same as `tyExp` but the return value is `void`. */
  void voidTyExp(unsigned int _n) {
    tyExp(_n);
  }

  void addParamsToLocalVarTypes(unsigned int _paramList) {
    Node paramList = m->get(_paramList);
    while (paramList.ty == NodeTy::PARAMLIST_CONS) {
      Node paramName = m->get(paramList.n1);
      std::string paramStr(paramName.extra.ptr, paramName.loc.sz);
      localVarTypes.add(paramStr, paramList.n2);
      paramList = m->get(paramList.n3);
    }
  }

  void tyFuncOrProc(unsigned int _n) {
    localVarTypes.push();
    Node n = m->get(_n);
    addParamsToLocalVarTypes(n.n2);
    unsigned int bodyType = tyExp(n.extra.nodes.n4);
    unify(bodyType, n.n3);
    localVarTypes.pop();
  }

  std::string tyNodeToString(unsigned int _ty) {
    Node ty = m->get(_ty);
    return std::string(NodeTyToString(ty.ty));
  }

  std::string unificationError(unsigned int _ty1, unsigned int _ty2) {
    return std::string("Cannot unify ") + tyNodeToString(_ty1) + " with " + tyNodeToString(_ty2);
  }

  std::string varNotFoundError(std::string &varName) {
    return std::string("Variable ") + varName + " is unbound";
  }

};

#endif
