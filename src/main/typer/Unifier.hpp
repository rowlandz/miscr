#ifndef TYPER_UNIFIER
#define TYPER_UNIFIER

#include <cassert>
#include "llvm/ADT/DenseMap.h"
#include "common/ASTContext.hpp"
#include "common/TypeContext.hpp"
#include "common/Ontology.hpp"
#include "common/ScopeStack.hpp"
#include "common/LocatedError.hpp"

/// @brief Second of the two type checking phases. Performs Hindley-Milner
/// type inference and unification. More specifically,
///
/// - Sets the type variables of expressions in an `ASTContext`. The type
///   variables can be resolved using the `TypeContext` built by the unifier.
/// 
/// - Fully qualifies the function names in `CALL` expressions (i.e. replaces
///   the name with a `FQIdent` with associated info stored in an `Ontology`).
class Unifier {
  
  ASTContext* ctx;
 
  /// Maps decls to their definitions or declarations.
  const Ontology* ont;

  /// Maps local variable names to type vars.
  ScopeStack<TVar> localVarTypes;

  /// Holds all scopes that could be used to fully-qualify a relative path.
  /// First scope is always "global", then each successive scope is an extension
  /// of the one before it. The last one is the "current scope".
  /// e.g., `{ "global", "global::MyMod", "global::MyMod::MyMod2" }`
  std::vector<std::string> relativePathQualifiers;

  /// Accumulates type checking errors.
  std::vector<LocatedError> errors;
  
  TypeContext tc;

  /// @brief Converts a type expression into a `Type` with fresh type variables.
  TVar freshFromTypeExp(Addr<TypeExp> _texp) {
    AST::ID id = ctx->get(_texp).getID();
    switch (ctx->get(_texp).getID()) {
    case AST::ID::ARRAY_TEXP: {
      ArrayTypeExp texp = ctx->GET_UNSAFE<ArrayTypeExp>(_texp);
      return tc.fresh(Type::array_sart(texp.getSize(),
        freshFromTypeExp(texp.getInnerType())));
    }
    case AST::ID::REF_TEXP:
      return tc.fresh(Type::rref(freshFromTypeExp(
        ctx->GET_UNSAFE<RefTypeExp>(_texp).getPointeeType()
      )));
    case AST::ID::WREF_TEXP:
      return tc.fresh(Type::wref(freshFromTypeExp(
        ctx->GET_UNSAFE<RefTypeExp>(_texp).getPointeeType()
      )));
    case AST::ID::BOOL_TEXP: return tc.fresh(Type::bool_());
    case AST::ID::f32_TEXP: return tc.fresh(Type::f32());
    case AST::ID::f64_TEXP: return tc.fresh(Type::f64());
    case AST::ID::i8_TEXP: return tc.fresh(Type::i8());
    case AST::ID::i16_TEXP: return tc.fresh(Type::i16());
    case AST::ID::i32_TEXP: return tc.fresh(Type::i32());
    case AST::ID::i64_TEXP: return tc.fresh(Type::i64());
    case AST::ID::STR_TEXP:
      return tc.fresh(Type::array_sart(Addr<Exp>::none(), tc.fresh(Type::i8())));
    case AST::ID::UNIT_TEXP: return tc.fresh(Type::unit());
    default: assert(false && "Unreachable code");
    }
  }

public:

  Unifier(ASTContext* ctx, Ontology* ont) {
    this->ctx = ctx;
    this->ont = ont;
    relativePathQualifiers.push_back("global");
  }

  const TypeContext* getTypeContext() const { return &tc; }

  const std::vector<LocatedError>* getErrors() { return &errors; }

  /// @brief Enforces an equality relation in `bindings` between the two type
  /// variables. Returns `true` if this is possible and `false` otherwise.
  /// @param implicitCoercions If this is set, then certain mismatches between
  /// the type vars are allowed to remain (such as from `wref` to `rref`).
  bool unify(TVar v1, TVar v2, bool implicitCoercions = false) {
    std::pair<TVar, Type> res1 = tc.resolve(v1);
    std::pair<TVar, Type> res2 = tc.resolve(v2);
    TVar w1 = res1.first;
    TVar w2 = res2.first;
    Type t1 = res1.second;
    Type t2 = res2.second;

    if (w1.get() == w2.get()) return true;
    if (t1.isNoType()) { tc.bind(w1, w2); return true; }
    if (t2.isNoType()) { tc.bind(w2, w1); return true; }

    if (t1.getID() == t2.getID()) {
      Type::ID ty = t1.getID();
      if (t1.isArrayType()) {
        return unify(t1.getInner(), t2.getInner());
      } else if (ty == Type::ID::REF || ty == Type::ID::RREF
      || ty == Type::ID::WREF) {
        if (!unify(t1.getInner(), t2.getInner())) return false;
        tc.bind(w1, w2);
        return true;
      } else if (ty == Type::ID::BOOL || ty == Type::ID::DECIMAL
      || ty == Type::ID::f32 || ty == Type::ID::f64 || ty == Type::ID::i8
      || ty == Type::ID::i16 || ty == Type::ID::i32 || ty == Type::ID::i64
      || ty == Type::ID::NUMERIC || ty == Type::ID::UNIT) {
        tc.bind(w1, w2);
        return true;
      }
      else return false;
    }
    else if (t1.isArrayType() && t2.isArrayType()) {
      return unify(t1.getInner(), t2.getInner());
    }
    else if (t1.getID() == Type::ID::NUMERIC) {
      switch (t2.getID()) {
      case Type::ID::DECIMAL: case Type::ID::f32: case Type::ID::f64:
      case Type::ID::i8: case Type::ID::i16: case Type::ID::i32:
      case Type::ID::i64:
        tc.bind(w1, w2);
        return true;
      default:
        return false;
      }
    }
    else if (t2.getID() == Type::ID::NUMERIC) {
      switch (t1.getID()) {
      case Type::ID::DECIMAL: case Type::ID::f32: case Type::ID::f64:
      case Type::ID::i8: case Type::ID::i16: case Type::ID::i32:
      case Type::ID::i64:
        tc.bind(w2, w1);
        return true;
      default:
        return false;
      }
    }
    else if (t1.getID() == Type::ID::DECIMAL) {
      switch (t2.getID()) {
      case Type::ID::f32: case Type::ID::f64:
        tc.bind(w1, w2);
        return true;
      default:
        return false;
      }
    }
    else if (t2.getID() == Type::ID::DECIMAL) {
      switch (t1.getID()) {
      case Type::ID::f32: case Type::ID::f64:
        tc.bind(w2, w1);
        return true;
      default:
        return false;
      }
    }
    else if (t1.getID() == Type::ID::REF) {
      switch (t2.getID()) {
      case Type::ID::RREF: case Type::ID::WREF:
        if (unify(t1.getInner(), t2.getInner())) {
          tc.bind(w1, w2);
          return true;
        } else return false;
      default:
        return false;
      }
    }
    else if (t2.getID() == Type::ID::REF) {
      switch (t1.getID()) {
      case Type::ID::RREF: case Type::ID::WREF:
        if (unify(t1.getInner(), t2.getInner())) {
          tc.bind(w2, w1);
          return true;
        } else return false;
      default:
        return false;
      }
    }
    else if (t1.getID() == Type::ID::WREF && implicitCoercions) {
      if (t2.getID() == Type::ID::RREF) {
        return unify(t1.getInner(), t2.getInner(), implicitCoercions);
      } else return false;
    }
    
    return false;
  }

  /// @brief Unifies `_exp`, then unifies the inferred type with `ty`. An error
  /// message is pushed if the second unification fails. 
  /// @return The inferred type of `_exp` (for convenience). 
  TVar unifyWith(Addr<Exp> _exp, TVar ty) {
    TVar inferredTy = unifyExp(_exp);
    if (unify(inferredTy, ty)) return inferredTy;
    std::string errMsg("Cannot unify type ");
    errMsg.append(tc.TVarToString(inferredTy));
    errMsg.append(" with type ");
    errMsg.append(tc.TVarToString(ty));
    errMsg.append(".");
    errors.push_back(LocatedError(ctx->get(_exp).getLocation(), errMsg));
    return inferredTy;
  }

  /// @brief Unifies `_exp`, then unifies the inferred type with `ty` with
  /// implicit coercions enabled. An error message is pushed if the second
  /// unification fails.
  /// @return The inferred type of `_exp` (for convenience).
  TVar expectTypeToBe(Addr<Exp> _exp, TVar expectedTy) {
    TVar inferredTy = unifyExp(_exp);
    if (unify(inferredTy, expectedTy, true)) return inferredTy;
    std::string errMsg("Inferred type is ");
    errMsg.append(tc.TVarToString(inferredTy));
    errMsg.append(" but expected type ");
    errMsg.append(tc.TVarToString(expectedTy));
    errMsg.append(".");
    errors.push_back(LocatedError(ctx->get(_exp).getLocation(), errMsg));
    return inferredTy;
  }

  // TODO: change if-stmt to switch-stmt
  /// @brief Unifies an expression or statement. Returns the type of `_e`. 
  /// Expressions or statements that bind local identifiers will cause
  /// `localVarTypes` to be updated.
  TVar unifyExp(Addr<Exp> _e) {
    AST::ID id = ctx->get(_e).getID();

    if (id == AST::ID::ADD || id == AST::ID::SUB || id == AST::ID::MUL
    || id == AST::ID::DIV) {
      BinopExp e = ctx->GET_UNSAFE<BinopExp>(_e);
      TVar lhsTy = unifyWith(e.getLHS(), tc.fresh(Type::numeric()));
      unifyWith(e.getRHS(), lhsTy);
      e.setTVar(lhsTy);
      ctx->set(_e, e);
    }

    else if (id == AST::ID::ARRAY_INIT) {
      ArrayInitExp e = ctx->GET_UNSAFE<ArrayInitExp>(_e);
      unifyWith(e.getSize(), tc.fresh(Type::numeric()));
      TVar ofTy = unifyExp(e.getInitializer());
      e.setTVar(tc.fresh(Type::array_sart(e.getSize(), ofTy)));
      ctx->set(_e, e);
    }

    else if (id == AST::ID::ARRAY_LIST) {
      ArrayListExp e = ctx->GET_UNSAFE<ArrayListExp>(_e);
      TVar ofTy = tc.fresh();
      ExpList expList = ctx->get(e.getContent());
      unsigned int numExps = 0;
      while (expList.nonEmpty()) {
        ++numExps;
        unifyWith(expList.getHead(), ofTy);
        expList = ctx->get(expList.getTail());
      }
      e.setTVar(tc.fresh(Type::array_sact(numExps, ofTy)));
      ctx->set(_e, e);
    }

    else if (id == AST::ID::ASCRIP) {
      AscripExp e = ctx->GET_UNSAFE<AscripExp>(_e);
      TVar ty = freshFromTypeExp(e.getAscripter());
      expectTypeToBe(e.getAscriptee(), ty);
      e.setTVar(ty);
      ctx->set(_e, e);
    }

    else if (id == AST::ID::BLOCK) {
      BlockExp e = ctx->GET_UNSAFE<BlockExp>(_e);
      ExpList stmtList = ctx->get(e.getStatements());
      TVar lastStmtTy = tc.fresh(Type::unit());  // TODO: no need to freshen unit
      localVarTypes.push();
      while (stmtList.nonEmpty()) {
        lastStmtTy = unifyExp(stmtList.getHead());
        stmtList = ctx->get(stmtList.getTail());
      }
      localVarTypes.pop();
      e.setTVar(lastStmtTy);
      ctx->set(_e, e);
    }

    else if (id == AST::ID::CALL) {
      CallExp e = ctx->GET_UNSAFE<CallExp>(_e);
      std::string calleeRelName = nameToString(e.getFunction());
      Addr<FunctionDecl> _callee = lookupFuncByRelName(calleeRelName);
      if (_callee.exists()) {
        FunctionDecl callee = ctx->get(_callee);
        /*** unify each argument with corresponding parameter ***/
        ExpList expList = ctx->get(e.getArguments());
        ParamList paramList = ctx->get(callee.getParameters());
        while (expList.nonEmpty() && paramList.nonEmpty()) {
          TVar paramType = freshFromTypeExp(paramList.getHeadParamType());
          expectTypeToBe(expList.getHead(), paramType);
          expList = ctx->get(expList.getTail());
          paramList = ctx->get(paramList.getTail());
        }
        if (expList.nonEmpty() || paramList.nonEmpty()) {
          FQIdent calleeFQIdent = ctx->GET_UNSAFE<FQIdent>(callee.getName());
          std::string errMsg("Arity mismatch for function ");
          errMsg.append(ont->getName(calleeFQIdent.getKey()) + ".");
          errors.push_back(LocatedError(e.getLocation(), errMsg));
        }
        /*** set this expression's type to the callee's return type ***/
        e.setTVar(freshFromTypeExp(callee.getReturnType()));
        /*** fully qualify the callee name ***/
        e.setFunction(callee.getName());
      } else {
        std::string errMsg = "Function " + calleeRelName + " not found.";
        errors.push_back(LocatedError(e.getLocation(), errMsg));
        e.setTVar(tc.fresh());
      }
      ctx->set(_e, e);
    }

    else if (id == AST::ID::DEC_LIT) {
      DecimalLit e = ctx->GET_UNSAFE<DecimalLit>(_e);
      e.setTVar(tc.fresh(Type::decimal()));
      ctx->set(_e, e);
    }

    else if (id == AST::ID::DEREF) {
      DerefExp e = ctx->GET_UNSAFE<DerefExp>(_e);
      TVar retTy = tc.fresh();
      TVar refTy = tc.fresh(Type::ref(retTy));
      unifyWith(e.getOf(), refTy);
      e.setTVar(retTy);
      ctx->set(_e, e);
    }

    // TODO: Make sure typevar is set even in error cases.
    else if (id == AST::ID::ENAME) {
      NameExp e = ctx->GET_UNSAFE<NameExp>(_e);
      Name name = ctx->get(e.getName());
      if (name.isUnqualified()) {
        std::string s = ctx->get(e.getName().UNSAFE_CAST<Ident>()).asString();
        TVar ty = localVarTypes.getOrElse(s, TVar::none());
        if (ty.exists()) e.setTVar(ty);
        else {
          errors.push_back(LocatedError(e.getLocation(), "Unbound identifier"));
        }
      } else {
        errors.push_back(LocatedError(e.getLocation(), "Didn't expect qualified ident here"));
      }
      ctx->set(_e, e);
    }

    else if (id == AST::ID::EQ || id == AST::ID::GE || id == AST::ID::GT
    || id == AST::ID::LE || id == AST::ID::LT || id == AST::ID::NE) {
      BinopExp e = ctx->GET_UNSAFE<BinopExp>(_e);
      TVar lhsTy = unifyExp(e.getLHS());
      unifyWith(e.getRHS(), lhsTy);
      e.setTVar(tc.fresh(Type::bool_()));
      ctx->set(_e, e);
    }

    else if (id == AST::ID::FALSE || id == AST::ID::TRUE) {
      BoolLit e = ctx->GET_UNSAFE<BoolLit>(_e);
      e.setTVar(tc.fresh(Type::bool_()));
      ctx->set(_e, e);
    }

    else if (id == AST::ID::IF) {
      IfExp e = ctx->GET_UNSAFE<IfExp>(_e);
      unifyWith(e.getCondExp(), tc.fresh(Type::bool_()));
      TVar thenTy = unifyExp(e.getThenExp());
      unifyWith(e.getElseExp(), thenTy);
      e.setTVar(thenTy);
      ctx->set(_e, e);
    }

    else if (id == AST::ID::INDEX) {
      IndexExp e = ctx->GET_UNSAFE<IndexExp>(_e);
      Type baseTy = tc.resolve(unifyExp(e.getBase())).second;
      Type::ID refTyID = baseTy.getID();
      if (refTyID == Type::ID::RREF || refTyID == Type::ID::WREF) {
        Type innerTy = tc.resolve(baseTy.getInner()).second;
        if (innerTy.isArrayType()) {
          e.setTVar(refTyID == Type::ID::WREF ?
                    tc.fresh(Type::wref(innerTy.getInner())) :
                    tc.fresh(Type::rref(innerTy.getInner())));
        } else {
          std::string errMsg("Must infer that this is a reference to an array");
          Location loc = ctx->get(e.getBase()).getLocation();
          errors.push_back(LocatedError(loc, errMsg));
          e.setTVar(tc.fresh());
        }
      } else {
        std::string errMsg("Couldn't infer that this is a reference");
        Location loc = ctx->get(e.getBase()).getLocation();
        errors.push_back(LocatedError(loc, errMsg));
        e.setTVar(tc.fresh());
      }
      unifyWith(e.getIndex(), tc.fresh(Type::numeric()));
      ctx->set(_e, e);
    }

    else if (id == AST::ID::INT_LIT) {
      IntLit e = ctx->GET_UNSAFE<IntLit>(_e);
      e.setTVar(tc.fresh(Type::numeric()));
      ctx->set(_e, e);
    }

    else if (id == AST::ID::LET) {
      LetExp e = ctx->GET_UNSAFE<LetExp>(_e);
      TVar rhsTy;
      if (e.getAscrip().exists()) {
        rhsTy = freshFromTypeExp(e.getAscrip());
        expectTypeToBe(e.getDefinition(), rhsTy);
      } else {
        rhsTy = unifyExp(e.getDefinition());
      }
      std::string boundIdent = ctx->get(e.getBoundIdent()).asString();
      localVarTypes.add(boundIdent, rhsTy);
      e.setTVar(tc.fresh(Type::unit()));
      ctx->set(_e, e);
    }

    else if (id == AST::ID::REF_EXP || id == AST::ID::WREF_EXP) {
      RefExp e = ctx->GET_UNSAFE<RefExp>(_e);
      TVar initializerTy = unifyExp(e.getInitializer());
      TVar retTy = id == AST::ID::WREF_EXP ?
                   tc.fresh(Type::wref(initializerTy)) :
                   tc.fresh(Type::rref(initializerTy));
      e.setTVar(retTy);
      ctx->set(_e, e);
    }

    else if (id == AST::ID::STORE) {
      StoreExp e = ctx->GET_UNSAFE<StoreExp>(_e);
      TVar retTy = tc.fresh();
      TVar lhsTy = tc.fresh(Type::wref(retTy));
      unifyWith(e.getLHS(), lhsTy);
      unifyWith(e.getRHS(), retTy);
      e.setTVar(retTy);
      ctx->set(_e, e);
    }

    else if (id == AST::ID::STRING_LIT) {
      StringLit e = ctx->GET_UNSAFE<StringLit>(_e);
      unsigned int strArrayLen = e.processEscapes().size() + 1;
      e.setTVar(tc.fresh(Type::rref(tc.fresh(Type::array_sact(strArrayLen,
        tc.fresh(Type::i8()))))));
      ctx->set(_e, e);
    }

    else {
      assert(false && "unimplemented");
    }

    return ctx->get(_e).getTVar(); 
  }

  /// Typechecks a function.
  void unifyFunc(Addr<FunctionDecl> _funDecl) {
    FunctionDecl funDecl = ctx->get(_funDecl);
    localVarTypes.push();
    addParamsToLocalVarTypes(funDecl.getParameters());
    TVar retTy = freshFromTypeExp(funDecl.getReturnType());
    unifyWith(funDecl.getBody(), retTy);
    localVarTypes.pop();
  }

  void unifyModule(Addr<ModuleDecl> _module) {
    ModuleDecl module = ctx->get(_module);
    FQIdent fqName = ctx->GET_UNSAFE<FQIdent>(module.getName());
    std::string fqNameStr = ont->getName(fqName.getKey());
    relativePathQualifiers.push_back(fqNameStr);
    unifyDeclList(module.getDecls());
    relativePathQualifiers.pop_back();
  }

  void unifyDecl(Addr<Decl> _decl) {
    switch (ctx->get(_decl).getID()) {
    case AST::ID::FUNC: unifyFunc(_decl.UNSAFE_CAST<FunctionDecl>()); break;
    case AST::ID::EXTERN_FUNC: break;
    case AST::ID::MODULE: unifyModule(_decl.UNSAFE_CAST<ModuleDecl>()); break;
    default: assert(false && "unimplemented");
    }
  }

  void unifyDeclList(Addr<DeclList> _declList) {
    DeclList declList = ctx->get(_declList);
    while (declList.nonEmpty()) {
      unifyDecl(declList.getHead());
      declList = ctx->get(declList.getTail());
    }
  }

  void addParamsToLocalVarTypes(Addr<ParamList> _paramList) {
    ParamList paramList = ctx->get(_paramList);
    while (paramList.nonEmpty()) {
      Ident paramName = ctx->get(paramList.getHeadParamName());
      TVar paramTy = freshFromTypeExp(paramList.getHeadParamType());
      localVarTypes.add(paramName.asString(), paramTy);
      paramList = ctx->get(paramList.getTail());
    }
  }

  /// Returns the name as a string (e.g., `MyMod1::MyMod2::myfunc`).
  std::string nameToString(Addr<Name> _name) {
    std::string ret;
    Name name = ctx->get(_name);
    while (name.isQualified()) {
      QIdent qident = ctx->GET_UNSAFE<QIdent>(_name);
      Ident part = ctx->get(qident.getHead());
      ret.append(part.asString());
      ret.append("::");
      _name = qident.getTail();
      name = ctx->get(qident.getTail());
    }
    Ident part = ctx->GET_UNSAFE<Ident>(_name);
    ret.append(part.asString());
    return ret;
  }

  Addr<FunctionDecl> lookupFuncByRelName(std::string relName) {
    for (auto iter = relativePathQualifiers.crbegin();
         iter != relativePathQualifiers.crend(); ++iter) {
      std::string fqName = *iter + "::" + relName;
      Addr<FunctionDecl> a = ont->getFunctionDecl(*iter + "::" + relName);
      if (a.exists()) return a;
    }
    return Addr<FunctionDecl>::none();
  }
};

#endif