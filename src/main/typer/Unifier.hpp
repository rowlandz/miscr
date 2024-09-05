#ifndef TYPER_UNIFIER
#define TYPER_UNIFIER

#include <string>
#include <map>
#include "common/Ontology.hpp"
#include "common/ScopeStack.hpp"
#include "common/NodeManager.hpp"
#include "common/LocatedError.hpp"

/** Second of three type checking phases.
 * Annotates expressions with types.
 * Fully-qualifies the names of called functions.
 * Fully-qualifies the names of all decls in their definitions.
 * TODO: Do we want to maintain the tree structure or is it okay to break it here?
 * Currently I'm assuming it's okay to break it.
 */
class Unifier {
public:

  NodeManager* m;
 
  /** Maps decls to their definitions or declarations. */
  const Ontology* ont;

  /** Maps local variable names to type nodes. */
  ScopeStack<unsigned int> localVarTypes;

  /** Holds all scopes that could be used to fully-qualify a relative path.
   * First scope is always "global", then each successive scope is an extension
   * of the one before it. The last one is the "current scope".
   * e.g., `{ "global", "global::MyMod", "global::MyMod::MyMod2" }`
   */
  std::vector<std::string> relativePathQualifiers;

  /** Accumulates type checking errors. */
  std::vector<LocatedError> errors;

  // Nodes are initialized for common types to avoid making multiple nodes.
  unsigned int ty_unit;
  unsigned int ty_bool;
  unsigned int ty_i8;
  unsigned int ty_i32;
  unsigned int ty_string;
  unsigned int tyc_numeric;
  unsigned int tyc_decimal;

  Unifier(NodeManager* m, Ontology* ont) {
    this->m = m;
    this->ont = ont;
    relativePathQualifiers.push_back(std::string("global"));
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
  bool unify(unsigned int _tyVar1, unsigned int _ty2) {
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
          return false;
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
          return false;
      }
    } else if (rty1.ty == NodeTy::DECIMAL) {
      switch (rty2.ty) {
        case NodeTy::f32:
        case NodeTy::f64:
          bind(_rtyVar1, _rty2);
          break;
        default:
          return false;
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
          return false;
      }
    } else {
      return false;
    }

    return true;
  }

  /** Typechecks `_exp` and unifies the inferred type with `_expectedTy`.
   * If the unification fails, pushes an error.
   * Returns the inferred type (same as tyExp). */
  unsigned int expectTyToBe(unsigned int _exp, unsigned int _expectedTy) {
    unsigned int _inferredTy = tyExp(_exp);
    if (!unify(_inferredTy, _expectedTy)) {
      std::string errMsg("I expected the type of this expression to be ");
      errMsg.append(tyNodeToString(resolve(_expectedTy)));
      errMsg.append(",\nbut I inferred the type to be ");
      errMsg.append(tyNodeToString(resolve(_inferredTy)));
      errMsg.append(".");
      errors.push_back(LocatedError(m->get(_exp).loc, errMsg));
    }
    return _inferredTy;
  }

  /** Typechecks an expression node at location `_n` in `m`.
   * Returns The type node `m->get(_n).n1` of the expression for convenience. */
  unsigned int tyExp(unsigned int _n) {
    Node n = m->get(_n);

    if (n.ty == NodeTy::ADD || n.ty == NodeTy::SUB || n.ty == NodeTy::MUL || n.ty == NodeTy::DIV) {
      unsigned int tyn2 = expectTyToBe(n.n2, tyc_numeric);
      unsigned int tyn3 = expectTyToBe(n.n3, tyn2);
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

    else if (n.ty == NodeTy::CALL) {
      Node eqident = m->get(n.n2);
      std::string calleeRelName = relQIdentToString(eqident.n2);
      bool yayFoundIt = false;
      for (auto qual = relativePathQualifiers.end()-1; relativePathQualifiers.begin() <= qual; qual--) {
        auto maybeFuncDecl = ont->findFunction(*qual + calleeRelName);
        if (maybeFuncDecl == ont->functionSpaceEnd()) continue;
        yayFoundIt = true;
        Node funcDecl = m->get(maybeFuncDecl->second);

        // Unify each of the arguments with parameter.
        Node expList = m->get(n.n3);
        Node paramList = m->get(funcDecl.n2);
        while (expList.ty == NodeTy::EXPLIST_CONS && paramList.ty == NodeTy::PARAMLIST_CONS) {
          expectTyToBe(expList.n1, paramList.n2);
          expList = m->get(expList.n2);
          paramList = m->get(paramList.n3);
        }
        if (expList.ty == NodeTy::EXPLIST_CONS || paramList.ty == NodeTy::PARAMLIST_CONS) {
          errors.push_back(LocatedError(paramList.loc, "Arity mismatch for function " + *qual + calleeRelName));
        }

        // Unify return type
        bind(n.n1, funcDecl.n3);

        // Replace function name with fully qualified name
        m->set(n.n2, { eqident.loc, NN, NN, NN, { .ptr = maybeFuncDecl->first }, NodeTy::EFQIDENT });
      }
      if (!yayFoundIt) {
        errors.push_back(LocatedError(eqident.loc, "Cannot find function " + calleeRelName));
      }
    }

    else if (n.ty == NodeTy::EQIDENT) {
      Node identNode = m->get(n.n2);
      if (identNode.ty == NodeTy::IDENT) {
        std::string identStr(identNode.extra.ptr, identNode.loc.sz);
        unsigned int varTyNode = localVarTypes.getOrElse(identStr, NN);
        if (varTyNode != NN) bind(n.n1, varTyNode);
        else errors.push_back(LocatedError(n.loc, "Unbound identifier."));
      } else {
        errors.push_back(LocatedError(n.loc, "Didn't expect qualified identifier here."));
      }
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
      printf("Unifier case not supported yet!: %s\n", NodeTyToString(n.ty));
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

  /** Typechecks a func, proc, extern func, or extern proc */
  void tyFuncOrProc(unsigned int _n) {
    Node n = m->get(_n);
    
    // fully qualify the function name
    Node funcNameNode = m->get(n.n1);
    std::string fqName = relativePathQualifiers.back() + "::" + std::string(funcNameNode.extra.ptr, funcNameNode.loc.sz);
    auto fqNamePerma = ont->getPermaPointer(fqName);
    m->set(n.n1, { funcNameNode.loc, NN, NN, NN, { .ptr = fqNamePerma }, NodeTy::FQIDENT });

    // type check the function body
    if (n.ty == NodeTy::FUNC || n.ty == NodeTy::PROC) {
      localVarTypes.push();
      addParamsToLocalVarTypes(n.n2);
      expectTyToBe(n.extra.nodes.n4, n.n3);
      localVarTypes.pop();
    }
  }

  // TODO: namespaces should be a little different
  void tyModuleOrNamespace(unsigned int _n) {
    Node n = m->get(_n);

    // fully qualify the module name
    Node modNameNode = m->get(n.n1);
    std::string fqName = relativePathQualifiers.back() + "::" + std::string(modNameNode.extra.ptr, modNameNode.loc.sz);
    auto fqNamePerma = ont->getPermaPointer(fqName);
    m->set(n.n1, { modNameNode.loc, NN, NN, NN, { .ptr = fqNamePerma }, NodeTy::FQIDENT });

    // recursively type check contents
    relativePathQualifiers.push_back(fqName);
    Node declList = m->get(n.n2);
    while (declList.ty == NodeTy::DECLLIST_CONS) {
      tyDecl(declList.n1);
      declList = m->get(declList.n2);
    }

    relativePathQualifiers.pop_back();
  }

  void tyDecl(unsigned int _n) {
    Node n = m->get(_n);

    if (n.ty == NodeTy::FUNC || n.ty == NodeTy::PROC
    || n.ty == NodeTy::EXTERN_FUNC || n.ty == NodeTy::EXTERN_PROC) {
      tyFuncOrProc(_n);
    }

    else if (n.ty == NodeTy::MODULE || n.ty == NodeTy::NAMESPACE) {
      tyModuleOrNamespace(_n);
    }
  }

  /** `_qident` must be the address of a QIDENT or IDENT.
   * Returns the (un)qualified name as a string prefixed with `::`.
   * e.g., `::MyMod::myfunc` */
  std::string relQIdentToString(unsigned int _qident) {
    std::string ret;
    Node qident = m->get(_qident);
    while (qident.ty == NodeTy::QIDENT) {
      Node part = m->get(qident.n1);
      ret.append("::");
      ret.append(std::string(part.extra.ptr, part.loc.sz));
      qident = m->get(qident.n2);
    }
    if (qident.ty == NodeTy::IDENT) {
      ret.append("::");
      ret.append(std::string(qident.extra.ptr, qident.loc.sz));
    }
    return ret;
  }

  std::string tyNodeToString(unsigned int _ty) {
    Node ty = m->get(_ty);
    return std::string(NodeTyToString(ty.ty));
  }

};

#endif
