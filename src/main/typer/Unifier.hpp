#ifndef TYPER_UNIFIERNEW
#define TYPER_UNIFIERNEW

#include <cassert>
#include "llvm/ADT/DenseMap.h"
#include "common/ASTContext.hpp"
#include "common/TypeContext.hpp"
#include "common/Ontology.hpp"
#include "common/ScopeStack.hpp"
#include "common/LocatedError.hpp"

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

public:
  /// Accumulates type checking errors.
  std::vector<LocatedError> errors;

private:
  
  TypeContext tc;

  TVar freshFromTypeExp(Addr<TypeExp> _tyexp) {
    AST::ID id = ctx->get(_tyexp).getID();
    switch (ctx->get(_tyexp).getID()) {
    case AST::ID::ARRAY_TEXP:
      return tc.fresh(Type::array(freshFromTypeExp(
        ctx->GET_UNSAFE<ArrayTypeExp>(_tyexp).getInnerType()
      )));
    case AST::ID::REF_TEXP:
      return tc.fresh(Type::ref(freshFromTypeExp(
        ctx->GET_UNSAFE<RefTypeExp>(_tyexp).getPointeeType()
      )));
    case AST::ID::WREF_TEXP:
      return tc.fresh(Type::wref(freshFromTypeExp(
        ctx->GET_UNSAFE<WRefTypeExp>(_tyexp).getPointeeType()
      )));
    case AST::ID::BOOL_TEXP: return tc.fresh(Type::bool_());
    case AST::ID::f32_TEXP: return tc.fresh(Type::f32());
    case AST::ID::f64_TEXP: return tc.fresh(Type::f64());
    case AST::ID::i8_TEXP: return tc.fresh(Type::i8());
    case AST::ID::i32_TEXP: return tc.fresh(Type::i32());
    case AST::ID::UNIT_TEXP: return tc.fresh(Type::unit());
    default:
      assert(false && "Unreachable code");
    }
  }

public:

  Unifier(ASTContext* ctx, Ontology* ont) {
    this->ctx = ctx;
    this->ont = ont;
    relativePathQualifiers.push_back("global");
  }

  const TypeContext* getTypeContext() const { return &tc; }

  /// @brief Enforces an equality relation in `bindings` between the two type
  /// variables. Returns `true` if this is possible and `false` otherwise.
  bool unify(TVar v1, TVar v2) {
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
      if (ty == Type::ID::ARRAY) {
        if (unify(t1.getInner(), t2.getInner())) {
          tc.bind(w1, w2);
          return true;
        } else return false;
      } else if (ty == Type::ID::REF || ty == Type::ID::WREF) {
        if (unify(t1.getInner(), t2.getInner())) {
          tc.bind(w1, w2);
          return true;
        } else return false;
      } else if (ty == Type::ID::BOOL || ty == Type::ID::DECIMAL
      || ty == Type::ID::f32 || ty == Type::ID::f64 || ty == Type::ID::i8
      || ty == Type::ID::i32 || ty == Type::ID::NUMERIC
      || ty == Type::ID::UNIT) {
        tc.bind(w1, w2);
        return true;
      }
      else return false;
    }
    else if (t1.getID() == Type::ID::NUMERIC) {
      switch (t2.getID()) {
      case Type::ID::DECIMAL: case Type::ID::f32: case Type::ID::f64:
      case Type::ID::i8: case Type::ID::i32:
        tc.bind(w1, w2);
        return true;
      default:
        return false;
      }
    }
    else if (t2.getID() == Type::ID::NUMERIC) {
      switch (t1.getID()) {
      case Type::ID::DECIMAL: case Type::ID::f32: case Type::ID::f64:
      case Type::ID::i8: case Type::ID::i32:
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
    
    return false;
  }

  /// @brief
  /// @return The inferred type of `_exp` (for convenience). 
  TVar expectTyToBe(Addr<Exp> _exp, TVar expectedTy) {
    TVar inferredTy = unifyExp(_exp);
    if (unify(inferredTy, expectedTy)) return inferredTy;
    std::string errMsg("Inferred type is ");
    errMsg.append(tc.TVarToString(inferredTy));
    errMsg.append(" but expected ");
    errMsg.append(tc.TVarToString(expectedTy));
    errMsg.append(".");
    errors.push_back(LocatedError(ctx->get(_exp).getLocation(), errMsg));
    return inferredTy;
  }

  /// @brief Unifies an expression or statement. Returns the type of `_e`. 
  /// Expressions or statements that bind local identifiers will cause
  /// `localVarTypes` to be updated.
  TVar unifyExp(Addr<Exp> _e) {
    AST::ID id = ctx->get(_e).getID();

    if (id == AST::ID::ADD || id == AST::ID::SUB || id == AST::ID::MUL
    || id == AST::ID::DIV) {
      BinopExp e = ctx->GET_UNSAFE<BinopExp>(_e);
      TVar lhsTy = expectTyToBe(e.getLHS(), tc.fresh(Type::numeric()));
      expectTyToBe(e.getRHS(), lhsTy);
      e.setTVar(lhsTy);
      ctx->SET_UNSAFE(_e, e);
    }

    else if (id == AST::ID::ASCRIP) {
      AscripExp e = ctx->GET_UNSAFE<AscripExp>(_e);
      TVar ty = freshFromTypeExp(e.getAscripter());
      expectTyToBe(e.getAscriptee(), ty);
      e.setTVar(ty);
      ctx->SET_UNSAFE(_e, e);
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
      ctx->SET_UNSAFE(_e, e);
    }

    // else if (id == AST::ID::CALL) {
    //   CallExp e = ctx->GET_UNSAFE<CallExp>(_e);
    //   std::string calleeName = nameToString(e.getFunction());
    //   bool yayFoundIt = false;
    //   for (auto qual = relativePathQualifiers.end()-1; relativePathQualifiers.begin() <= qual; qual--) {
    //     Addr<FunctionDecl> _funcDecl = ont->findFunction(*qual + "::" + calleeName);
    //     if (_funcDecl.isError()) continue;
    //     yayFoundIt = true;
    //     FunctionDecl funcDecl = ctx->get(_funcDecl);

    //     // Unify each of the arguments with parameter.
    //     ExpList expList = ctx->get(e.getArguments());
    //     ParamList paramList = ctx->get(funcDecl.getParameters());
    //     while (expList.nonEmpty() && paramList.nonEmpty()) {
    //       expectTyToBe(expList.getHead(), freshFromTypeExp(paramList.getHeadParamType()));
    //       expList = ctx->get(expList.getTail());
    //       paramList = ctx->get(paramList.getTail());
    //     }
    //     if (expList.nonEmpty() || paramList.nonEmpty()) {
    //       errors.push_back(LocatedError(expList.getLocation(), "Arity mismatch for function " + *qual + calleeName));
    //     }

    //     // Unify return type
    //     e.setTVar(freshFromTypeExp(funcDecl.getReturnType()));

    //     // Replace function name with fully qualified name
        
    //   }
    //   if (!yayFoundIt) {
    //     errors.push_back(LocatedError(e.getLocation(), "Cannot find function " + calleeName));
    //   }
    // }

    else if (id == AST::ID::DEC_LIT) {
      DecimalLit e = ctx->GET_UNSAFE<DecimalLit>(_e);
      e.setTVar(tc.fresh(Type::decimal()));
      ctx->SET_UNSAFE(_e, e);
    }

    else if (id == AST::ID::DEREF) {
      DerefExp e = ctx->GET_UNSAFE<DerefExp>(_e);
      TVar retTy = tc.fresh();
      TVar refTy = tc.fresh(Type::ref(retTy));
      expectTyToBe(e.getOf(), refTy);
      e.setTVar(retTy);
      ctx->SET_UNSAFE(_e, e);
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
      ctx->SET_UNSAFE(_e, e);
    }

    else if (id == AST::ID::EQ || id == AST::ID::NE) {
      BinopExp e = ctx->GET_UNSAFE<BinopExp>(_e);
      TVar lhsTy = unifyExp(e.getLHS());
      expectTyToBe(e.getRHS(), lhsTy);
      e.setTVar(tc.fresh(Type::bool_()));
      ctx->SET_UNSAFE(_e, e);
    }

    else if (id == AST::ID::FALSE || id == AST::ID::TRUE) {
      BoolLit e = ctx->GET_UNSAFE<BoolLit>(_e);
      e.setTVar(tc.fresh(Type::bool_()));
      ctx->SET_UNSAFE(_e, e);
    }

    else if (id == AST::ID::IF) {
      IfExp e = ctx->GET_UNSAFE<IfExp>(_e);
      expectTyToBe(e.getCondExp(), tc.fresh(Type::bool_()));
      TVar thenTy = unifyExp(e.getThenExp());
      expectTyToBe(e.getElseExp(), thenTy);
      e.setTVar(thenTy);
      ctx->SET_UNSAFE(_e, e);
    }

    else if (id == AST::ID::INT_LIT) {
      IntLit e = ctx->GET_UNSAFE<IntLit>(_e);
      e.setTVar(tc.fresh(Type::numeric()));
      ctx->SET_UNSAFE(_e, e);
    }

    else if (id == AST::ID::LET) {
      LetExp e = ctx->GET_UNSAFE<LetExp>(_e);
      TVar rhsTy = unifyExp(e.getDefinition());
      std::string boundIdent = ctx->get(e.getBoundIdent()).asString();
      localVarTypes.add(boundIdent, rhsTy);
      e.setTVar(tc.fresh(Type::unit()));
      ctx->SET_UNSAFE(_e, e);
    }

    else if (id == AST::ID::REF_EXP) {
      RefExp e = ctx->GET_UNSAFE<RefExp>(_e);
      TVar initializerTy = unifyExp(e.getInitializer());
      e.setTVar(tc.fresh(Type::ref(initializerTy)));
      ctx->SET_UNSAFE(_e, e);
    }

    else if (id == AST::ID::STORE) {
      StoreExp e = ctx->GET_UNSAFE<StoreExp>(_e);
      TVar retTy = tc.fresh();
      TVar lhsTy = tc.fresh(Type::ref(retTy));
      expectTyToBe(e.getLHS(), lhsTy);
      expectTyToBe(e.getRHS(), retTy);
      e.setTVar(retTy);
      ctx->SET_UNSAFE(_e, e);
    }

    else {
      assert(false && "unimplemented");
    }

    return ctx->get(_e).getTVar(); 
  }

  /// Typechecks a func, extern func.
  void unifyFunc(Addr<FunctionDecl> _funDecl) {
    FunctionDecl funDecl = ctx->get(_funDecl);
    localVarTypes.push();
    addParamsToLocalVarTypes(funDecl.getParameters());
    TVar retTy = freshFromTypeExp(funDecl.getReturnType());
    expectTyToBe(funDecl.getBody(), retTy);
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
    AST::ID id = ctx->get(_decl).getID();
    if (id == AST::ID::FUNC || id == AST::ID::EXTERN_FUNC)
      unifyFunc(_decl.UNSAFE_CAST<FunctionDecl>());
    else if (id == AST::ID::MODULE)
      unifyModule(_decl.UNSAFE_CAST<ModuleDecl>());
    else
      assert(false && "unimplemented");
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
      name = ctx->get(qident.getTail());
    }
    Ident part = ctx->GET_UNSAFE<Ident>(_name);
    ret.append(part.asString());
    return ret;
  }

  // std::pair<std::string, Addr<FunctionDecl>>
  // lookupFunction(std::string relName) {
  //   for (auto iter = relativePathQualifiers.crbegin();
  //        iter != relativePathQualifiers.crend(); ++iter) {
  //     std::string fqName = *iter + "::" + relName;
  //     Addr<FunctionDecl> a = ont->findFunction(*iter + "::" + relName);
  //     if (a.exists()) return a;
  //   }
  //   return Addr<FunctionDecl>::none();
  // }
};

#endif