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
/// - Sets the type variables of expressions. The type
///   variables can be resolved using the `TypeContext` built by the unifier.
/// 
/// - Fully qualifies the function names in `CALL` expressions (i.e. replaces
///   the name with a `FQIdent` with associated info stored in an `Ontology`).
class Unifier {
 
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
  TVar freshFromTypeExp(TypeExp* _texp) {
    AST::ID id = _texp->getID();

    if (auto texp = ArrayTypeExp::downcast(_texp)) {
      return tc.fresh(Type::array_sart(texp->getSize(),
        freshFromTypeExp(texp->getInnerType())));
    }
    if (auto texp = RefTypeExp::downcast(_texp)) {
      if (texp->isWritable())
        return tc.fresh(Type::wref(freshFromTypeExp(texp->getPointeeType())));
      else
        return tc.fresh(Type::rref(freshFromTypeExp(texp->getPointeeType())));
    }
    if (id == AST::ID::BOOL_TEXP) return tc.fresh(Type::bool_());
    if (id == AST::ID::f32_TEXP) return tc.fresh(Type::f32());
    if (id == AST::ID::f64_TEXP) return tc.fresh(Type::f64());
    if (id == AST::ID::i8_TEXP) return tc.fresh(Type::i8());
    if (id == AST::ID::i16_TEXP) return tc.fresh(Type::i16());
    if (id == AST::ID::i32_TEXP) return tc.fresh(Type::i32());
    if (id == AST::ID::i64_TEXP) return tc.fresh(Type::i64());
    if (id == AST::ID::STR_TEXP)
      return tc.fresh(Type::array_sart(nullptr, tc.fresh(Type::i8())));
    if (id == AST::ID::UNIT_TEXP) return tc.fresh(Type::unit());
    llvm_unreachable("Ahhh!!!");
  }

public:

  Unifier(Ontology* ont) {
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
  TVar unifyWith(Exp* _exp, TVar ty) {
    TVar inferredTy = unifyExp(_exp);
    if (unify(inferredTy, ty)) return inferredTy;
    std::string errMsg("Cannot unify type ");
    errMsg.append(tc.TVarToString(inferredTy));
    errMsg.append(" with type ");
    errMsg.append(tc.TVarToString(ty));
    errMsg.append(".");
    errors.push_back(LocatedError(_exp->getLocation(), errMsg));
    return inferredTy;
  }

  /// @brief Unifies `_exp`, then unifies the inferred type with `ty` with
  /// implicit coercions enabled. An error message is pushed if the second
  /// unification fails.
  /// @return The inferred type of `_exp` (for convenience).
  TVar expectTypeToBe(Exp* _exp, TVar expectedTy) {
    TVar inferredTy = unifyExp(_exp);
    if (unify(inferredTy, expectedTy, true)) return inferredTy;
    std::string errMsg("Inferred type is ");
    errMsg.append(tc.TVarToString(inferredTy));
    errMsg.append(" but expected type ");
    errMsg.append(tc.TVarToString(expectedTy));
    errMsg.append(".");
    errors.push_back(LocatedError(_exp->getLocation(), errMsg));
    return inferredTy;
  }

  // TODO: change if-stmt to switch-stmt
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

    else if (auto e = ArrayInitExp::downcast(_e)) {
      unifyWith(e->getSize(), tc.fresh(Type::numeric()));
      TVar ofTy = unifyExp(e->getInitializer());
      e->setTVar(tc.fresh(Type::array_sart(e->getSize(), ofTy)));
    }

    else if (auto e = ArrayListExp::downcast(_e)) {
      TVar ofTy = tc.fresh();
      llvm::ArrayRef<Exp*> content = e->getContent()->asArrayRef();
      for (Exp* innerE : content) unifyWith(innerE, ofTy);
      e->setTVar(tc.fresh(Type::array_sact(content.size(), ofTy)));
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
      std::string calleeRelName = e->getFunction()->asStringRef().str();
      FunctionDecl* callee = lookupFuncByRelName(calleeRelName);
      if (callee != nullptr) {
        /*** unify each argument with corresponding parameter ***/
        auto expList = e->getArguments()->asArrayRef();
        auto paramList = callee->getParameters()->asArrayRef();
        int idx = 0;
        while (idx < expList.size() && idx < paramList.size()) {
          TVar paramType = freshFromTypeExp(paramList[idx].second);
          expectTypeToBe(expList[idx], paramType);
          ++idx;
        }
        if (idx < expList.size() || idx < paramList.size()) {
          Name* calleeFQIdent = callee->getName();
          llvm::Twine errMsg = "Arity mismatch for function " +
            callee->getName()->asStringRef() + ".";
          errors.push_back(LocatedError(e->getLocation(), errMsg.str()));
        }
        /*** set this expression's type to the callee's return type ***/
        e->setTVar(freshFromTypeExp(callee->getReturnType()));
        /*** fully qualify the callee name ***/
        e->setFunction(callee->getName());
      } else {
        std::string errMsg = "Function " + calleeRelName + " not found.";
        errors.push_back(LocatedError(e->getLocation(), errMsg));
        e->setTVar(tc.fresh());
      }
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
      else
        errors.push_back(LocatedError(e->getLocation(), "Unbound identifier"));
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
      Type baseTy = tc.resolve(unifyExp(e->getBase())).second;
      Type::ID refTyID = baseTy.getID();
      if (refTyID == Type::ID::RREF || refTyID == Type::ID::WREF) {
        Type innerTy = tc.resolve(baseTy.getInner()).second;
        if (innerTy.isArrayType()) {
          e->setTVar(refTyID == Type::ID::WREF ?
                    tc.fresh(Type::wref(innerTy.getInner())) :
                    tc.fresh(Type::rref(innerTy.getInner())));
        } else {
          std::string errMsg("Must infer that this is a reference to an array");
          errors.push_back(LocatedError(e->getBase()->getLocation(), errMsg));
          e->setTVar(tc.fresh());
        }
      } else {
        std::string errMsg("Couldn't infer that this is a reference");
        errors.push_back(LocatedError(e->getBase()->getLocation(), errMsg));
        e->setTVar(tc.fresh());
      }
      unifyWith(e->getIndex(), tc.fresh(Type::numeric()));
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
      TVar retTy = id == AST::ID::WREF_EXP ?
                   tc.fresh(Type::wref(initializerTy)) :
                   tc.fresh(Type::rref(initializerTy));
      e->setTVar(retTy);
    }

    else if (auto e = StoreExp::downcast(_e)) {
      TVar retTy = tc.fresh();
      TVar lhsTy = tc.fresh(Type::wref(retTy));
      unifyWith(e->getLHS(), lhsTy);
      unifyWith(e->getRHS(), retTy);
      e->setTVar(retTy);
    }

    else if (auto e = StringLit::downcast(_e)) {
      unsigned int strArrayLen = e->processEscapes().size() + 1;
      e->setTVar(tc.fresh(Type::rref(tc.fresh(Type::array_sact(strArrayLen,
        tc.fresh(Type::i8()))))));
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
    relativePathQualifiers.push_back(mod->getName()->asStringRef().str());
    unifyDeclList(mod->getDecls());
    relativePathQualifiers.pop_back();
  }

  void unifyDecl(Decl* _decl) {
    if (auto func = FunctionDecl::downcast(_decl))
      unifyFunc(func);
    else if (auto mod = ModuleDecl::downcast(_decl))
      unifyModule(mod);
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

  FunctionDecl* lookupFuncByRelName(std::string relName) {
    for (auto iter = relativePathQualifiers.crbegin();
         iter != relativePathQualifiers.crend(); ++iter) {
      std::string fqName = *iter + "::" + relName;
      FunctionDecl* a = ont->getFunction(*iter + "::" + relName);
      if (a != nullptr) return a;
    }
    return nullptr;
  }
};

#endif