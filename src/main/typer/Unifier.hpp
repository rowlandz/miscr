#ifndef TYPER_UNIFIERNEW
#define TYPER_UNIFIERNEW

#include <cassert>
#include "llvm/ADT/DenseMap.h"
#include "common/ASTContext.hpp"
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
  
  struct MyDenseMapInfo {
    static inline TVar getEmptyKey() { return TVar(-1); }
    static inline TVar getTombstoneKey() { return TVar(-2); }
    static unsigned getHashValue(const TVar &Val) { return Val.get(); }
    static bool isEqual(const TVar &LHS, const TVar &RHS) {
      return LHS.get() == RHS.get();
    }
  };

  /// Type variable bindings.
  llvm::DenseMap<TVar, TypeOrTVar, struct MyDenseMapInfo> bindings;

  int firstUnusedTypeVar = 1;

  TVar fresh(Type ty) {
    bindings[TVar(firstUnusedTypeVar)] = TypeOrTVar(ty);
    return firstUnusedTypeVar++;
  }

  TVar freshFromTypeExp(Addr<TypeExp> _tyexp) {
    AST::ID id = ctx->get(_tyexp).getID();
    switch (ctx->get(_tyexp).getID()) {
    case AST::ID::ARRAY_TEXP:
      return fresh(Type::array(freshFromTypeExp(
        ctx->GET_UNSAFE<ArrayTypeExp>(_tyexp).getInnerType()
      )));
    case AST::ID::REF_TEXP:
      return fresh(Type::ref(freshFromTypeExp(
        ctx->GET_UNSAFE<RefTypeExp>(_tyexp).getPointeeType()
      )));
    case AST::ID::WREF_TEXP:
      return fresh(Type::wref(freshFromTypeExp(
        ctx->GET_UNSAFE<WRefTypeExp>(_tyexp).getPointeeType()
      )));
    case AST::ID::BOOL_TEXP: return fresh(Type::bool_());
    case AST::ID::f32_TEXP: return fresh(Type::f32());
    case AST::ID::f64_TEXP: return fresh(Type::f64());
    case AST::ID::i8_TEXP: return fresh(Type::i8());
    case AST::ID::i32_TEXP: return fresh(Type::i32());
    case AST::ID::UNIT_TEXP: return fresh(Type::unit());
    default:
      assert(false && "Unreachable code");
    }
  }

  /// @brief Follows `bindings` until a type var is found that is either bound
  /// to a type or bound do nothing. In the latter case, `Type::notype()` is
  /// returned. Side-effect free. 
  std::pair<TVar, Type> resolve(TVar v) {
    auto found = bindings.find(v);
    while (found != bindings.end()) {
      if (found->getSecond().isType()) {
        return std::pair<TVar, Type>(v, found->getSecond().getType());
      }
      v = found->getSecond().getTVar();
      found = bindings.find(v);
    }
    return std::pair<TVar, Type>(v, Type::notype());
  }

public:

  Unifier(ASTContext* ctx, Ontology* ont) {
    this->ctx = ctx;
    this->ont = ont;
    relativePathQualifiers.push_back("global");
  }

  /// @brief Enforces an equality relation in `bindings` between the two type
  /// variables. Returns `true` if this is possible and `false` otherwise.
  bool unify(TVar v1, TVar v2) {
    std::pair<TVar, Type> res1 = resolve(v1);
    std::pair<TVar, Type> res2 = resolve(v2);
    TVar w1 = res1.first;
    TVar w2 = res2.first;
    Type t1 = res1.second;
    Type t2 = res2.second;

    if (t1.isNoType()) { bindings[w1] = TypeOrTVar(w2); return true; }
    if (t2.isNoType()) { bindings[w2] = TypeOrTVar(w1); return true; }

    if (t1.getID() == t2.getID()) {
      Type::ID ty = t1.getID();
      if (ty == Type::ID::ARRAY) {
        if (unify(t1.getInner(), t2.getInner())) {
          bindings[w1] = TypeOrTVar(w2);
          return true;
        } else return false;
      } else if (ty == Type::ID::REF || ty == Type::ID::WREF) {
        if (unify(t1.getInner(), t2.getInner())) {
          bindings[w1] = TypeOrTVar(w2);
          return true;
        } else return false;
      } else if (ty == Type::ID::BOOL || ty == Type::ID::DECIMAL
      || ty == Type::ID::f32 || ty == Type::ID::f64 || ty == Type::ID::i8
      || ty == Type::ID::i32 || ty == Type::ID::NUMERIC
      || ty == Type::ID::UNIT) {
        bindings[w1] = TypeOrTVar(w2);
        return true;
      }
      else return false;
    }
    else if (t1.getID() == Type::ID::NUMERIC) {
      switch (t2.getID()) {
      case Type::ID::DECIMAL: case Type::ID::f32: case Type::ID::f64:
      case Type::ID::i8: case Type::ID::i32:
        bindings[w1] = TypeOrTVar(w2);
        return true;
      default:
        return false;
      }
    }
    else if (t2.getID() == Type::ID::NUMERIC) {
      switch (t1.getID()) {
      case Type::ID::DECIMAL: case Type::ID::f32: case Type::ID::f64:
      case Type::ID::i8: case Type::ID::i32:
        bindings[w2] = TypeOrTVar(w1);
        return true;
      default:
        return false;
      }
    }
    else if (t1.getID() == Type::ID::DECIMAL) {
      switch (t2.getID()) {
      case Type::ID::f32: case Type::ID::f64:
        bindings[w1] = TypeOrTVar(w2);
        return true;
      default:
        return false;
      }
    }
    else if (t2.getID() == Type::ID::DECIMAL) {
      switch (t1.getID()) {
      case Type::ID::f32: case Type::ID::f64:
        bindings[w2] = TypeOrTVar(w1);
        return true;
      default:
        return false;
      }
    }
    
    return false;
  }

  std::string TVarToString(TVar v) {
    auto res = resolve(v);
    if (res.second.isNoType()) {
      return std::string("?") + std::to_string(v.get());
    } else {
      switch (res.second.getID()) {
      case Type::ID::ARRAY:
        return std::string("array<") + TVarToString(res.second.getInner()) + ">";
      case Type::ID::REF:
        return std::string("ref<") + TVarToString(res.second.getInner()) + ">";
      case Type::ID::WREF:
        return std::string("wref<") + TVarToString(res.second.getInner()) + ">";
      case Type::ID::BOOL: return std::string("bool");
      case Type::ID::DECIMAL: return std::string("decimal");
      case Type::ID::f32: return std::string("f32");
      case Type::ID::f64: return std::string("f64");
      case Type::ID::i8: return std::string("i8");
      case Type::ID::i32: return std::string("i32");
      case Type::ID::NUMERIC: return std::string("numeric");
      case Type::ID::UNIT: return std::string("unit");
      default: return std::string("???");
      }
    }
  }

  /// @brief
  /// @return The inferred type of `_exp` (for convenience). 
  TVar expectTyToBe(Addr<Exp> _exp, TVar expectedTy) {
    TVar inferredTy = unifyExp(_exp);
    if (unify(inferredTy, expectedTy)) return inferredTy;
    std::string errMsg("Inferred type is ");
    errMsg.append(TVarToString(inferredTy));
    errMsg.append(" but expected ");
    errMsg.append(TVarToString(expectedTy));
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
      TVar lhsTy = expectTyToBe(e.getLHS(), fresh(Type::numeric()));
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
      TVar lastStmtTy = fresh(Type::unit());  // TODO: no need to freshen unit
      localVarTypes.push();
      while (stmtList.nonEmpty()) {
        lastStmtTy = unifyExp(stmtList.getHead());
        stmtList = ctx->get(stmtList.getTail());
      }
      localVarTypes.pop();
      e.setTVar(lastStmtTy);
      ctx->SET_UNSAFE(_e, e);
    }

    else if (id == AST::ID::CALL) {
      CallExp e = ctx->GET_UNSAFE<CallExp>(_e);
      std::string calleeRelName = relNameToString(e.getFunction());
      bool yayFoundIt = false;
      for (auto qual = relativePathQualifiers.end()-1; relativePathQualifiers.begin() <= qual; qual--) {
        auto maybeFuncDecl = ont->findFunction(*qual + calleeRelName);
        if (maybeFuncDecl == ont->functionSpaceEnd()) continue;
        yayFoundIt = true;
        FunctionDecl funcDecl = ctx->get(maybeFuncDecl->second);

        // Unify each of the arguments with parameter.
        ExpList expList = ctx->get(e.getArguments());
        ParamList paramList = ctx->get(funcDecl.getParameters());
        while (expList.nonEmpty() && paramList.nonEmpty()) {
          expectTyToBe(expList.getHead(), freshFromTypeExp(paramList.getHeadParamType()));
          expList = ctx->get(expList.getTail());
          paramList = ctx->get(paramList.getTail());
        }
        if (expList.nonEmpty() || paramList.nonEmpty()) {
          errors.push_back(LocatedError(expList.getLocation(), "Arity mismatch for function " + *qual + calleeRelName));
        }

        // Unify return type
        e.setTVar(freshFromTypeExp(funcDecl.getReturnType()));

        // Replace function name with fully qualified name
        // TODO: this
      }
      if (!yayFoundIt) {
        errors.push_back(LocatedError(e.getLocation(), "Cannot find function " + calleeRelName));
      }
    }

    else if (id == AST::ID::DEC_LIT) {
      DecimalLit e = ctx->GET_UNSAFE<DecimalLit>(_e);
      e.setTVar(fresh(Type::decimal()));
      ctx->SET_UNSAFE(_e, e);
    }

    /* TODO: Make sure typevar is set even in error cases. */
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
      e.setTVar(fresh(Type::bool_()));
      ctx->SET_UNSAFE(_e, e);
    }

    else if (id == AST::ID::FALSE || id == AST::ID::TRUE) {
      BoolLit e = ctx->GET_UNSAFE<BoolLit>(_e);
      e.setTVar(fresh(Type::bool_()));
      ctx->SET_UNSAFE(_e, e);
    }

    else if (id == AST::ID::IF) {
      IfExp e = ctx->GET_UNSAFE<IfExp>(_e);
      expectTyToBe(e.getCondExp(), fresh(Type::bool_()));
      TVar thenTy = unifyExp(e.getThenExp());
      expectTyToBe(e.getElseExp(), thenTy);
      e.setTVar(thenTy);
      ctx->SET_UNSAFE(_e, e);
    }

    else if (id == AST::ID::INT_LIT) {
      IntLit e = ctx->GET_UNSAFE<IntLit>(_e);
      e.setTVar(fresh(Type::numeric()));
      ctx->SET_UNSAFE(_e, e);
    }

    else if (id == AST::ID::LET) {
      LetExp e = ctx->GET_UNSAFE<LetExp>(_e);
      TVar rhsTy = unifyExp(e.getDefinition());
      std::string boundIdent = ctx->get(e.getBoundIdent()).asString();
      localVarTypes.add(boundIdent, rhsTy);
      return fresh(Type::unit());
    }

    else if (id == AST::ID::REF_EXP) {
      RefExp e = ctx->GET_UNSAFE<RefExp>(_e);
      TVar initializerTy = unifyExp(e.getInitializer());
      e.setTVar(fresh(Type::ref(initializerTy)));
      ctx->SET_UNSAFE(_e, e);
    }

    else {
      assert(false && "unimplemented");
    }

    return ctx->get(_e).getTVar(); 
  }

  /// Returns the (un)qualified name as a string prefixed with `::`.
  /// e.g., `::MyMod::myfunc`
  std::string relNameToString(Addr<Name> _name) {
    std::string ret;
    Name name = ctx->get(_name);
    while (name.isQualified()) {
      QIdent qident = ctx->GET_UNSAFE<QIdent>(_name);
      Ident part = ctx->get(qident.getHead());
      ret.append("::");
      ret.append(part.asString());
      name = ctx->get(qident.getTail());
    }
    Ident part = ctx->GET_UNSAFE<Ident>(_name);
    ret.append("::");
    ret.append(part.asString());
    return ret;
  }
};

#endif