#ifndef TYPER_UNIFIER
#define TYPER_UNIFIER

#include <cassert>
#include "llvm/ADT/DenseMap.h"
#include "common/TypeContext.hpp"
#include "common/Ontology.hpp"
#include "common/ScopeStack.hpp"
#include "common/LocatedError.hpp"

/// @brief Second of the two type checking phases. Performs Hindley-Milner
/// type inference and unification. More specifically,
///
/// - Sets the type variables of expressions in the AST. The type
///   variables can be resolved using the `TypeContext` built by the unifier.
class Unifier {
 
  /// Maps decls to their definitions or declarations.
  const Ontology& ont;

  /// Maps local variable names to type vars.
  ScopeStack<TVar> localVarTypes;

  /// Accumulates type checking errors.
  std::vector<LocatedError>& errors;
  
  TypeContext tc;

  /// @brief Converts a type expression into a `Type` with fresh type variables.
  TVar freshFromTypeExp(TypeExp* _texp) {
    AST::ID id = _texp->getID();

    if (auto texp = NameTypeExp::downcast(_texp)) {
      return tc.fresh(Type::name(texp->getName()));
    }
    if (auto texp = RefTypeExp::downcast(_texp)) {
      return tc.fresh(Type::ref(freshFromTypeExp(texp->getPointeeType())));
    }
    if (id == AST::ID::BOOL_TEXP) return tc.fresh(Type::bool_());
    if (id == AST::ID::f32_TEXP) return tc.fresh(Type::f32());
    if (id == AST::ID::f64_TEXP) return tc.fresh(Type::f64());
    if (id == AST::ID::i8_TEXP) return tc.fresh(Type::i8());
    if (id == AST::ID::i16_TEXP) return tc.fresh(Type::i16());
    if (id == AST::ID::i32_TEXP) return tc.fresh(Type::i32());
    if (id == AST::ID::i64_TEXP) return tc.fresh(Type::i64());
    if (id == AST::ID::UNIT_TEXP) return tc.fresh(Type::unit());
    llvm_unreachable("Ahhh!!!");
  }

public:

  Unifier(Ontology& ont, std::vector<LocatedError>& errors)
    : ont(ont), errors(errors) {}

  const TypeContext& getTypeContext() const { return tc; }

  const std::vector<LocatedError>* getErrors() { return &errors; }

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
      if (ty == Type::ID::REF) {
        if (!unify(t1.getInner(), t2.getInner())) return false;
        tc.bind(w1, w2);
        return true;
      } else if (ty == Type::ID::NAME) {
        if (t1.getName()->asStringRef() != t2.getName()->asStringRef())
          return false;
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
    return false;
  }

  /// @brief Unifies `_exp`, then unifies the inferred type with `ty`. An error
  /// message is pushed if the second unification fails. 
  /// @return The inferred type of `_exp` (for convenience). 
  TVar unifyWith(Exp* _exp, TVar ty) {
    TVar inferredTy = unifyExp(_exp);
    if (unify(inferredTy, ty)) return inferredTy;
    LocatedError err;
    err.appendStatic("Cannot unify ");
    err.append(tc.TVarToString(inferredTy));
    err.appendStatic(" with type ");
    err.append(tc.TVarToString(ty));
    err.appendStatic(".\n");
    err.append(_exp->getLocation());
    errors.push_back(err);
    return inferredTy;
  }

  /// @brief Unifies `_exp`, then unifies the inferred type with `ty`. An error
  /// message is pushed if the second unification fails.
  /// @return The inferred type of `_exp` (for convenience).
  TVar expectTypeToBe(Exp* _exp, TVar expectedTy) {
    TVar inferredTy = unifyExp(_exp);
    if (unify(inferredTy, expectedTy)) return inferredTy;
    LocatedError err;
    err.appendStatic("Inferred type is ");
    err.append(tc.TVarToString(inferredTy));
    err.appendStatic(" but expected type ");
    err.append(tc.TVarToString(expectedTy));
    err.appendStatic(".\n");
    err.append(_exp->getLocation());
    errors.push_back(err);
    return inferredTy;
  }

  /// @brief Unifies an expression or statement. Returns the type of `_e`. 
  /// Expressions or statements that bind local identifiers will cause
  /// `localVarTypes` to be updated.
  TVar unifyExp(Exp* _e) {
    AST::ID id = _e->getID();

    if (auto e = BinopExp::downcast(_e)) {
      if (id == AST::ID::ADD || id == AST::ID::SUB || id == AST::ID::MUL
      || id == AST::ID::DIV) {
        TVar lhsTy = unifyWith(e->getLHS(), tc.fresh(Type::numeric()));
        unifyWith(e->getRHS(), lhsTy);
        e->setTVar(lhsTy);
      }

      else if (id == AST::ID::EQ || id == AST::ID::GE || id == AST::ID::GT
      || id == AST::ID::LE || id == AST::ID::LT || id == AST::ID::NE) {
        TVar lhsTy = unifyExp(e->getLHS());
        unifyWith(e->getRHS(), lhsTy);
        e->setTVar(tc.fresh(Type::bool_()));
      }
    }

    else if (auto e = AscripExp::downcast(_e)) {
      TVar ty = freshFromTypeExp(e->getAscripter());
      expectTypeToBe(e->getAscriptee(), ty);
      e->setTVar(ty);
    }

    else if (auto e = BlockExp::downcast(_e)) {
      llvm::ArrayRef<Exp*> stmtList = e->getStatements()->asArrayRef();
      TVar lastStmtTy = tc.fresh(Type::unit());  // TODO: no need to freshen unit
      localVarTypes.push();
      for (Exp* stmt : stmtList) lastStmtTy = unifyExp(stmt);
      localVarTypes.pop();
      e->setTVar(lastStmtTy);
    }

    else if (auto e = CallExp::downcast(_e)) {
      llvm::StringRef calleeName = e->getFunction()->asStringRef();
      Decl* _callee = ont.getDecl(calleeName, Ontology::Space::FUNCTION_OR_TYPE);
      assert (_callee != nullptr && "Bug in cataloger or canonicalizer.");
      if (auto callee = FunctionDecl::downcast(_callee)) {
        /*** unify each argument with corresponding parameter ***/
        auto argList = e->getArguments()->asArrayRef();
        auto paramList = callee->getParameters()->asArrayRef();
        int idx = 0;
        while (idx < argList.size() && idx < paramList.size()) {
          TVar paramType = freshFromTypeExp(paramList[idx].second);
          expectTypeToBe(argList[idx], paramType);
          ++idx;
        }
        if (idx < argList.size() || idx < paramList.size()) {
          LocatedError err;
          err.appendStatic("Arity mismatch for function ");
          err.append(callee->getName()->asStringRef());
          err.appendStatic(". Expected ");
          err.append(std::to_string(paramList.size()));
          err.appendStatic(" but got ");
          err.append(std::to_string(argList.size()));
          err.appendStatic(".\n");
          err.append(e->getLocation());
          errors.push_back(err);
        }
        /*** set this expression's type to the callee's return type ***/
        e->setTVar(freshFromTypeExp(callee->getReturnType()));
      }
      else if (auto callee = DataDecl::downcast(_callee)) {
        auto args = e->getArguments()->asArrayRef();
        auto params = callee->getFields()->asArrayRef();
        int idx = 0;
        while (idx < args.size() && idx < params.size()) {
          TVar paramType = freshFromTypeExp(params[idx].second);
          expectTypeToBe(args[idx], paramType);
          ++idx;
        }
        if (idx < args.size() || idx < params.size()) {
          LocatedError err;
          err.appendStatic("Arity mismatch for constructor ");
          err.append(callee->getName()->asStringRef());
          err.appendStatic(". Expected ");
          err.append(std::to_string(params.size()));
          err.appendStatic(" but got ");
          err.append(std::to_string(args.size()));
          err.appendStatic(".\n");
          err.append(e->getLocation());
          errors.push_back(err);
        }
        e->setTVar(tc.fresh(Type::name(callee->getName())));
      }
      else llvm_unreachable("Unexpected callee");
    }

    else if (auto e = DecimalLit::downcast(_e)) {
      e->setTVar(tc.fresh(Type::decimal()));
    }

    else if (auto e = DerefExp::downcast(_e)) {
      TVar retTy = tc.fresh();
      TVar refTy = tc.fresh(Type::ref(retTy));
      unifyWith(e->getOf(), refTy);
      e->setTVar(retTy);
    }

    // TODO: Make sure typevar is set even in error cases.
    else if (auto e = NameExp::downcast(_e)) {
      Name* name = e->getName();
      std::string s = e->getName()->asStringRef().str();
      TVar ty = localVarTypes.getOrElse(s, TVar::none());
      if (ty.exists())
        e->setTVar(ty);
      else {
        LocatedError err;
        err.appendStatic("Unbound identifier.\n");
        err.append(e->getLocation());
        errors.push_back(err);
      }
    }

    else if (auto e = BoolLit::downcast(_e)) {
      e->setTVar(tc.fresh(Type::bool_()));
    }

    else if (auto e = IfExp::downcast(_e)) {
      unifyWith(e->getCondExp(), tc.fresh(Type::bool_()));
      TVar thenTy = unifyExp(e->getThenExp());
      unifyWith(e->getElseExp(), thenTy);
      e->setTVar(thenTy);
    }

    else if (auto e = IndexExp::downcast(_e)) {
      TVar baseTy = unifyWith(e->getBase(), tc.fresh(Type::ref(tc.fresh())));
      unifyWith(e->getIndex(), tc.fresh(Type::numeric()));
      e->setTVar(baseTy);
    }

    else if (auto e = IntLit::downcast(_e)) {
      e->setTVar(tc.fresh(Type::numeric()));
    }

    else if (auto e = LetExp::downcast(_e)) {
      TVar rhsTy;
      if (e->getAscrip() != nullptr) {
        rhsTy = freshFromTypeExp(e->getAscrip());
        expectTypeToBe(e->getDefinition(), rhsTy);
      } else {
        rhsTy = unifyExp(e->getDefinition());
      }
      std::string boundIdent = e->getBoundIdent()->asStringRef().str();
      localVarTypes.add(boundIdent, rhsTy);
      e->setTVar(tc.fresh(Type::unit()));
    }

    else if (auto e = RefExp::downcast(_e)) {
      TVar initializerTy = unifyExp(e->getInitializer());
      e->setTVar(tc.fresh(Type::ref(initializerTy)));
    }

    else if (auto e = StoreExp::downcast(_e)) {
      TVar retTy = tc.fresh();
      TVar lhsTy = tc.fresh(Type::ref(retTy));
      unifyWith(e->getLHS(), lhsTy);
      unifyWith(e->getRHS(), retTy);
      e->setTVar(retTy);
    }

    else if (auto e = StringLit::downcast(_e)) {
      e->setTVar(tc.fresh(Type::ref(tc.fresh(Type::i8()))));
    }

    else {
      assert(false && "unimplemented");
    }

    return _e->getTVar(); 
  }

  /// Typechecks a function.
  void unifyFunc(FunctionDecl* func) {
    if (func->getID() == AST::ID::EXTERN_FUNC) return;
    localVarTypes.push();
    addParamsToLocalVarTypes(func->getParameters());
    TVar retTy = freshFromTypeExp(func->getReturnType());
    unifyWith(func->getBody(), retTy);
    localVarTypes.pop();
  }

  void unifyModule(ModuleDecl* mod) {
    unifyDeclList(mod->getDecls());
  }

  void unifyDecl(Decl* _decl) {
    if (auto func = FunctionDecl::downcast(_decl))
      unifyFunc(func);
    else if (auto mod = ModuleDecl::downcast(_decl))
      unifyModule(mod);
    else if (_decl->getID() == AST::ID::DATA)
      {}
    else
      llvm_unreachable("Didn't recognize decl");
  }

  void unifyDeclList(DeclList* declList) {
    for (Decl* decl : declList->asArrayRef()) unifyDecl(decl);
  }

  void addParamsToLocalVarTypes(ParamList* paramList) {
    for (auto param : paramList->asArrayRef()) {
      std::string paramName = param.first->asStringRef().str();
      TVar paramTy = freshFromTypeExp(param.second);
      localVarTypes.add(paramName, paramTy);
    }
  }
};

#endif