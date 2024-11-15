#ifndef TYPER_UNIFIER
#define TYPER_UNIFIER

#include <cassert>
#include "llvm/ADT/DenseMap.h"
#include "common/LocatedError.hpp"
#include "common/Ontology.hpp"
#include "common/ScopeStack.hpp"
#include "common/TypeContext.hpp"

/// @brief Third of three type checking phases. Performs Hindley-Milner type
/// inference and unification.
///
/// Every expression is labeled with a TVar. These type variables can be
/// resolved into Type objects using a TypeContext.
class Unifier { 
  const Ontology& ont;
  TypeContext& tc;
  std::vector<LocatedError>& errors;

  /// Maps local variable names to type vars.
  ScopeStack<TVar> localVarTypes;

public:
  Unifier(Ontology& ont, TypeContext& tc, std::vector<LocatedError>& errors)
    : ont(ont), tc(tc), errors(errors) {}
  Unifier(const Unifier&) = delete;

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
      if (ty == Type::ID::BREF) {
        if (!unify(t1.getInner(), t2.getInner())) return false;
        tc.bind(w1, w2);
        return true;
      } else if (ty == Type::ID::OREF) {
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
    errors.push_back(LocatedError()
      << "Cannot unify " << tc.TVarToString(inferredTy) << " with type "
      << tc.TVarToString(ty) << ".\n" << _exp->getLocation()
    );
    return inferredTy;
  }

  /// @brief Unifies `_exp`, then unifies the inferred type with `ty`. An error
  /// message is pushed if the second unification fails.
  /// @return The inferred type of `_exp` (for convenience).
  TVar expectTypeToBe(Exp* _exp, TVar expectedTy) {
    TVar inferredTy = unifyExp(_exp);
    if (unify(inferredTy, expectedTy)) return inferredTy;
    errors.push_back(LocatedError()
      << "Inferred type is " << tc.TVarToString(inferredTy)
      << " but expected type " << tc.TVarToString(expectedTy) << ".\n"
      << _exp->getLocation()
    );
    return inferredTy;
  }

  /// @brief Unifies an expression or statement. Returns the type of `_e`. 
  /// Expressions or statements that bind local identifiers will cause
  /// `localVarTypes` to be updated.
  TVar unifyExp(Exp* _e) {
    AST::ID id = _e->id;

    if (auto e = AscripExp::downcast(_e)) {
      TVar ty = tc.freshFromTypeExp(e->getAscripter());
      expectTypeToBe(e->getAscriptee(), ty);
      e->setTVar(ty);
    }

    else if (auto e = BinopExp::downcast(_e)) {
      switch (e->getBinop()) {
      case BinopExp::ADD: case BinopExp::SUB: case BinopExp::MUL:
      case BinopExp::DIV: case BinopExp::MOD: {
        TVar lhsTy = unifyWith(e->getLHS(), tc.fresh(Type::numeric()));
        unifyWith(e->getRHS(), lhsTy);
        e->setTVar(lhsTy);
        break; 
      }
      case BinopExp::AND: case BinopExp::OR: {
        TVar boolTy = tc.fresh(Type::bool_());
        unifyWith(e->getLHS(), boolTy);
        unifyWith(e->getRHS(), boolTy);
        e->setTVar(boolTy);
        break;
      }
      case BinopExp::EQ: case BinopExp::NE: case BinopExp::GE:
      case BinopExp::GT: case BinopExp::LE: case BinopExp::LT: {
        TVar lhsTy = unifyExp(e->getLHS());
        unifyWith(e->getRHS(), lhsTy);
        e->setTVar(tc.fresh(Type::bool_()));
      }
      }
    }

    else if (auto e = BlockExp::downcast(_e)) {
      llvm::ArrayRef<Exp*> stmtList = e->getStatements()->asArrayRef();
      TVar lastStmtTy = tc.fresh(Type::unit());  // TODO: no need to freshen unit
      localVarTypes.push();
      for (Exp* stmt : stmtList) lastStmtTy = unifyExp(stmt);
      localVarTypes.pop();
      e->setTVar(lastStmtTy);
    }

    else if (auto e = BoolLit::downcast(_e)) {
      e->setTVar(tc.fresh(Type::bool_()));
    }

    else if (auto e = BorrowExp::downcast(_e)) {
      TVar innerTy = tc.fresh();
      expectTypeToBe(e->getRefExp(), tc.fresh(Type::oref(innerTy)));
      e->setTVar(tc.fresh(Type::bref(innerTy)));
    }

    else if (auto e = CallExp::downcast(_e)) {
      unifyCallExp(e);
    }

    else if (auto e = DecimalLit::downcast(_e)) {
      e->setTVar(tc.fresh(Type::decimal()));
    }

    else if (auto e = DerefExp::downcast(_e)) {
      TVar retTy = tc.fresh();
      TVar refTy = tc.fresh(Type::bref(retTy));
      unifyWith(e->getOf(), refTy);
      e->setTVar(retTy);
    }

    // TODO: Make sure typevar is set even in error cases.
    else if (auto e = NameExp::downcast(_e)) {
      std::string s = e->getName()->asStringRef().str();
      TVar ty = localVarTypes.getOrElse(s, TVar::none());
      if (ty.exists()) {
        e->setTVar(ty);
      } else {
        errors.push_back(LocatedError()
          << "Unbound identifier.\n" << e->getLocation()
        );
        e->setTVar(tc.fresh());
      }
    }

    else if (auto e = IfExp::downcast(_e)) {
      unifyWith(e->getCondExp(), tc.fresh(Type::bool_()));
      TVar thenTy = unifyExp(e->getThenExp());
      unifyWith(e->getElseExp(), thenTy);
      e->setTVar(thenTy);
    }

    else if (auto e = IndexExp::downcast(_e)) {
      TVar baseTy = unifyWith(e->getBase(), tc.fresh(Type::bref(tc.fresh())));
      unifyWith(e->getIndex(), tc.fresh(Type::numeric()));
      e->setTVar(baseTy);
    }

    else if (auto e = IntLit::downcast(_e)) {
      e->setTVar(tc.fresh(Type::numeric()));
    }

    else if (auto e = LetExp::downcast(_e)) {
      TVar rhsTy;
      if (e->getAscrip() != nullptr) {
        rhsTy = tc.freshFromTypeExp(e->getAscrip());
        expectTypeToBe(e->getDefinition(), rhsTy);
      } else {
        rhsTy = unifyExp(e->getDefinition());
      }
      std::string boundIdent = e->getBoundIdent()->asStringRef().str();
      localVarTypes.add(boundIdent, rhsTy);
      e->setTVar(tc.fresh(Type::unit()));
    }

    else if (auto e = MoveExp::downcast(_e)) {
      TVar retTy = tc.fresh(Type::oref(tc.fresh()));
      expectTypeToBe(e->getRefExp(), tc.fresh(Type::bref(retTy)));
      e->setTVar(retTy);
    }

    else if (auto e = ProjectExp::downcast(_e)) {
      unifyProjectExp(e);
    }

    else if (auto e = RefExp::downcast(_e)) {
      TVar initializerTy = unifyExp(e->getInitializer());
      e->setTVar(tc.fresh(Type::bref(initializerTy)));
    }

    else if (auto e = StoreExp::downcast(_e)) {
      TVar retTy = tc.fresh();
      TVar lhsTy = tc.fresh(Type::bref(retTy));
      unifyWith(e->getLHS(), lhsTy);
      unifyWith(e->getRHS(), retTy);
      e->setTVar(tc.fresh(Type::unit()));
    }

    else if (auto e = StringLit::downcast(_e)) {
      e->setTVar(tc.fresh(Type::bref(tc.fresh(Type::i8()))));
    }

    else if (auto e = UnopExp::downcast(_e)) {
      switch (e->getUnop()) {
      case UnopExp::NEG:
        e->setTVar(unifyWith(e->getInner(), tc.fresh(Type::numeric())));
        break;
      case UnopExp::NOT:
        e->setTVar(unifyWith(e->getInner(), tc.fresh(Type::bool_())));
        break;
      }
    }

    else {
      llvm_unreachable("Unifier::unifyExp() unexpected case");
    }

    return _e->getTVar(); 
  }

  /// @brief Unifies a function call or constructor call expression.
  void unifyCallExp(CallExp* e) {
    llvm::StringRef calleeName = e->getFunction()->asStringRef();
    llvm::ArrayRef<Exp*> args = e->getArguments()->asArrayRef();
    llvm::ArrayRef<std::pair<Name*, TypeExp*>> params;
    bool variadic;

    // get params, variadic, and set return type
    Decl* calleeDecl = ont.getFunctionOrConstructor(calleeName);
    if (auto callee = FunctionDecl::downcast(calleeDecl)) {
      params = callee->getParameters()->asArrayRef();
      variadic = callee->isVariadic();
      e->setTVar(tc.freshFromTypeExp(callee->getReturnType()));
    } else if (auto callee = DataDecl::downcast(calleeDecl)) {
      params = callee->getFields()->asArrayRef();
      variadic = false;
      e->setTVar(tc.fresh(Type::name(callee->getName())));
    }

    // check for arity mismatch
    if (!variadic && args.size() != params.size())
      errors.push_back(LocatedError()
        << "Arity mismatch for function " << calleeName << ". Expected "
        << std::to_string(params.size()) << " arguments but got "
        << std::to_string(args.size()) << ".\n" << e->getLocation()
      );
    else if (variadic && args.size() < params.size())
      errors.push_back(LocatedError()
        << "Arity mismatch for function " << calleeName << ". Expected at "
        << "least " << std::to_string(params.size()) << " arguments but got "
        << std::to_string(args.size()) << ".\n" << e->getLocation()
      );

    // unify arguments
    for (int i = 0; i < args.size(); ++i) {
      if (i >= params.size()) unifyExp(args[i]);
      else unifyWith(args[i], tc.freshFromTypeExp(params[i].second));
    }
  }

  /// @brief Unifies a projection expression. 
  void unifyProjectExp(ProjectExp* e) {
    ProjectExp::Kind kind = e->getKind();
    TVar dataTVar = tc.fresh();
    
    // unify the base expression
    unifyWith(e->getBase(),
      kind == ProjectExp::DOT ? dataTVar : tc.fresh(Type::bref(dataTVar)));
    
    // lookup the data type decl from the inferred type of base
    Type dataType = tc.resolve(dataTVar).second;
    if (dataType.getID() != Type::ID::NAME) {
      errors.push_back(LocatedError()
        << "Could not infer what data type is being indexed.\n"
        << e->getBase()->getLocation()
      );
      e->setTVar(tc.fresh());
      return;
    }
    DataDecl* dd = ont.getType(dataType.getName()->asStringRef());
    
    // set the data type name in e (for convenience)
    e->setTypeName(dd->getName()->asStringRef());

    // lookup the field type
    llvm::StringRef field = e->getFieldName()->asStringRef();
    TypeExp* fieldTExp = dd->getFields()->findParamType(field);
    if (fieldTExp == nullptr) {
      errors.push_back(LocatedError()
        << field << " is not a field of data type "
        << dd->getName()->asStringRef() << ".\n" << e->getLocation()
      );
      e->setTVar(tc.fresh());
      return;
    }
    TVar fieldTy = tc.freshFromTypeExp(fieldTExp);

    // set the type of e
    e->setTVar(
      kind == ProjectExp::BRACKETS ? tc.fresh(Type::bref(fieldTy)) : fieldTy);
  }

  /// Typechecks a function.
  void unifyFunc(FunctionDecl* func) {
    if (func->isExtern()) return;
    localVarTypes.push();
    addParamsToLocalVarTypes(func->getParameters());
    TVar retTy = tc.freshFromTypeExp(func->getReturnType());
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
    else if (_decl->id == AST::ID::DATA)
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
      TVar paramTy = tc.freshFromTypeExp(param.second);
      localVarTypes.add(paramName, paramTy);
    }
  }
};

#endif