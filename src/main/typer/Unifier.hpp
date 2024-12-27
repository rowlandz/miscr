#ifndef TYPER_UNIFIER
#define TYPER_UNIFIER

#include <cassert>
#include "llvm/ADT/DenseMap.h"
#include "common/LocatedError.hpp"
#include "common/Ontology.hpp"
#include "common/ScopeStack.hpp"
#include "common/TypeContext.hpp"

/// @brief Third of five type checking phases. Performs Hindley-Milner type
/// inference and unification. Sets the `type` field of all Exp.
///
/// The heart of unification is a variation of the union-find algorithm over
/// type variables (TypeVar). An expression like `x + y` requires the types
/// of `x` and `y` to be equal (i.e., their types must be _unified_). This
/// technique allows type information to seemingly "propagate backwards" (e.g.,
/// the type checker infers that `1` in `1 + (2: i32)` must be an `i32`.
///
/// The `union` operation of union-find has two complications resulting from
/// the binding of type variables to actual types:
///   - Two typevars can only be unioned if they're bound to compatible types
///     or constraints (e.g., i32 and numeric, but not &i32 and numeric).
///   - Unioning two typevars may recursively require unioning other typevars
///     (e.g., unioning reference types requires unioning the inner types).
class Unifier { 
  const Ontology& ont;
  TypeContext& tc;
  std::vector<LocatedError>& errors;

  /// @brief Defines the equivalence classes for type variables in the
  /// union-find algorithm. A type variable that is _not_ a key in this map is
  /// the _representative_ for its equivalence class.
  llvm::DenseMap<TypeVar*, TypeVar*>& tvarEquiv;

  /// @brief Maps type variables (that are representatives of their equivalence
  /// classes) to a type or type constraint. A representative that is _not_
  /// present here is considered _unconstrained_ (like the `nothing` type).
  /// A type variable may _not_ be bound to another type variable.
  llvm::DenseMap<TypeVar*, Type*>& tvarBindings;

  /// @brief Maps names of local identifiers to their types.
  ScopeStack<Type*> localVarTypes;

public:
  Unifier(Ontology& ont, TypeContext& tc,
          llvm::DenseMap<TypeVar*, TypeVar*>& tvarEquiv,
          llvm::DenseMap<TypeVar*, Type*>& tvarBindings,
          std::vector<LocatedError>& errors)
    : ont(ont), tc(tc), tvarEquiv(tvarEquiv), tvarBindings(tvarBindings),
      errors(errors) {}

  Unifier(const Unifier&) = delete;

  void unifyDeclList(DeclList* declList)
    { for (Decl* decl : declList->asArrayRef()) unifyDecl(decl); }

  void unifyDecl(Decl* _decl) {
    if (auto func = FunctionDecl::downcast(_decl)) unifyFunc(func);
    else if (auto mod = ModuleDecl::downcast(_decl)) unifyModule(mod);
    else if (StructDecl::downcast(_decl)) {}
    else llvm_unreachable("Didn't recognize decl");
  }

  void unifyModule(ModuleDecl* mod) { unifyDeclList(mod->getDecls()); }

  void unifyFunc(FunctionDecl* func) {
    if (func->isExtern()) return;
    localVarTypes.push();
    addParamsToLocalVarTypes(func->getParameters());
    Type* retTy = tc.getTypeFromTypeExp(func->getReturnType());
    expectTypeToBe(func->getBody(), retTy);
    localVarTypes.pop();
  }

  /// @brief Unifies an expression or statement. Returns the type of @p _e. 
  /// Expressions or statements that bind local identifiers will cause
  /// `localVarTypes` to be updated.
  Type* unifyExp(Exp* _e) {
    AST::ID id = _e->id;

    if (auto e = AddrOfExp::downcast(_e)) {
      Type* ofTy = unifyExp(e->getOf());
      e->setType(tc.getRefType(ofTy, false));
    }

    else if (auto e = AscripExp::downcast(_e)) {
      Type* ty = tc.getTypeFromTypeExp(e->getAscripter());
      expectTypeToBe(e->getAscriptee(), ty);
      e->setType(ty);
    }

    else if (auto e = AssignExp::downcast(_e)) {
      Type* lhsTy = unifyExp(e->getLHS());
      expectTypeToBe(e->getRHS(), lhsTy);
      e->setType(tc.getUnit());
    }

    else if (auto e = BinopExp::downcast(_e)) {
      switch (e->getBinop()) {
      case BinopExp::ADD: case BinopExp::SUB: case BinopExp::MUL:
      case BinopExp::DIV: case BinopExp::MOD: {
        Type* lhsTy = expectTypeToBe(e->getLHS(), tc.getNumeric());
        expectTypeToBe(e->getRHS(), lhsTy);
        e->setType(lhsTy);
        break; 
      }
      case BinopExp::AND: case BinopExp::OR: {
        Type* boolType = tc.getBool();
        expectTypeToBe(e->getLHS(), boolType);
        expectTypeToBe(e->getRHS(), boolType);
        e->setType(boolType);
        break;
      }
      case BinopExp::EQ: case BinopExp::NE: case BinopExp::GE:
      case BinopExp::GT: case BinopExp::LE: case BinopExp::LT: {
        Type* lhsTy = unifyExp(e->getLHS());
        expectTypeToBe(e->getRHS(), lhsTy);
        e->setType(tc.getBool());
      }
      }
    }

    else if (auto e = BlockExp::downcast(_e)) {
      llvm::ArrayRef<Exp*> stmtList = e->getStatements()->asArrayRef();
      Type* lastStmtTy = tc.getUnit();
      localVarTypes.push();
      for (Exp* stmt : stmtList) lastStmtTy = unifyExp(stmt);
      localVarTypes.pop();
      e->setType(lastStmtTy);
    }

    else if (auto e = BoolLit::downcast(_e)) {
      e->setType(tc.getBool());
    }

    else if (auto e = BorrowExp::downcast(_e)) {
      TypeVar* innerTy = tc.getFreshTypeVar();
      expectTypeToBe(e->getRefExp(), tc.getRefType(innerTy, true));
      e->setType(tc.getRefType(innerTy, false));
    }

    else if (auto e = CallExp::downcast(_e)) {
      unifyCallExp(e);
    }

    else if (auto e = ConstrExp::downcast(_e)) {
      unifyConstrExp(e);
    }

    else if (auto e = DecimalLit::downcast(_e)) {
      TypeVar* v = tc.getFreshTypeVar();
      bind(v, tc.getDecimal());
      e->setType(v);
    }

    else if (auto e = DerefExp::downcast(_e)) {
      TypeVar* retTy = tc.getFreshTypeVar();
      expectTypeToBe(e->getOf(), tc.getRefType(retTy, false));
      e->setType(retTy);
    }

    else if (auto e = NameExp::downcast(_e)) {
      std::string s = e->getName()->asStringRef().str();
      if (Type* ty = localVarTypes.getOrElse(s, nullptr)) {
        e->setType(ty);
      } else {
        errors.push_back(LocatedError()
          << "Unbound identifier.\n" << e->getLocation()
        );
        e->setType(tc.getFreshTypeVar());
      }
    }

    else if (auto e = IfExp::downcast(_e)) {
      expectTypeToBe(e->getCondExp(), tc.getBool());
      if (Exp* elseExp = e->getElseExp()) {
        Type* thenTy = unifyExp(e->getThenExp());
        expectTypeToBe(elseExp, thenTy);
        e->setType(thenTy);
      } else {
        expectTypeToBe(e->getThenExp(), tc.getUnit());
        e->setType(tc.getUnit());
      }
    }

    else if (auto e = IndexExp::downcast(_e)) {
      Type* refTy = tc.getRefType(tc.getFreshTypeVar(), false);
      Type* baseTy = expectTypeToBe(e->getBase(), refTy);
      expectTypeToBe(e->getIndex(), tc.getNumeric());
      e->setType(baseTy);
    }

    else if (auto e = IntLit::downcast(_e)) {
      TypeVar* ty = tc.getFreshTypeVar();
      bind(ty, tc.getNumeric());
      e->setType(ty);
    }

    else if (auto e = LetExp::downcast(_e)) {
      Type* rhsTy;
      if (TypeExp* ascr = e->getAscrip()) {
        rhsTy = expectTypeToBe(e->getDefinition(), tc.getTypeFromTypeExp(ascr));
      } else {
        rhsTy = unifyExp(e->getDefinition());
      }
      std::string boundIdent = e->getBoundIdent()->asStringRef().str();
      localVarTypes.add(boundIdent, rhsTy);
      e->setType(tc.getUnit());
    }

    else if (auto e = MoveExp::downcast(_e)) {
      Type* retTy = tc.getRefType(tc.getFreshTypeVar(), true);
      expectTypeToBe(e->getRefExp(), retTy);
      e->setType(retTy);
    }

    else if (auto e = ProjectExp::downcast(_e)) {
      unifyProjectExp(e);
    }

    else if (auto e = StringLit::downcast(_e)) {
      e->setType(tc.getRefType(tc.getI8(), false));
    }

    else if (auto e = UnopExp::downcast(_e)) {
      switch (e->getUnop()) {
      case UnopExp::NEG: {
        TypeVar* ty = tc.getFreshTypeVar();
        bind(ty, tc.getNumeric());
        e->setType(expectTypeToBe(e->getInner(), ty));
        break;
      }
      case UnopExp::NOT:
        e->setType(expectTypeToBe(e->getInner(), tc.getBool()));
        break;
      }
    }

    else if (auto e = WhileExp::downcast(_e)) {
      expectTypeToBe(e->getCond(), tc.getBool());
      expectTypeToBe(e->getBody(), tc.getUnit());
      e->setType(tc.getUnit());
    }

    else {
      llvm_unreachable("Unifier::unifyExp() unexpected case");
    }

    return _e->getType();
  }

  /// @brief Unifies a function call expression.
  void unifyCallExp(CallExp* e) {
    llvm::StringRef calleeName = e->getFunction()->asStringRef();
    llvm::ArrayRef<Exp*> args = e->getArguments()->asArrayRef();
    llvm::ArrayRef<std::pair<Name*, TypeExp*>> params;
    bool variadic;

    // get params, variadic, and set return type
    FunctionDecl* calleeDecl = ont.getFunction(calleeName);
    params = calleeDecl->getParameters()->asArrayRef();
    variadic = calleeDecl->isVariadic();
    e->setType(tc.getTypeFromTypeExp(calleeDecl->getReturnType()));

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
      else expectTypeToBe(args[i], tc.getTypeFromTypeExp(params[i].second));
    }
  }

  /// @brief Unifies a struct constructor expression.
  void unifyConstrExp(ConstrExp* e) {
    llvm::StringRef calleeName = e->getStruct()->asStringRef();
    llvm::ArrayRef<Exp*> args = e->getFields()->asArrayRef();
    llvm::ArrayRef<std::pair<Name*, TypeExp*>> fields;

    // get params and set return type
    StructDecl* calleeDecl = ont.getType(calleeName);
    fields = calleeDecl->getFields()->asArrayRef();
    e->setType(tc.getNameType(calleeDecl->getName()->asStringRef()));

    // check for arity mismatch
    if (args.size() != fields.size())
      errors.push_back(LocatedError()
        << "Arity mismatch for constructor " << calleeName << ". Expected "
        << std::to_string(fields.size()) << " fields but got "
        << std::to_string(args.size()) << ".\n" << e->getLocation()
      );

    // unify arguments
    for (int i = 0; i < args.size(); ++i) {
      if (i >= fields.size()) unifyExp(args[i]);
      else expectTypeToBe(args[i], tc.getTypeFromTypeExp(fields[i].second));
    }
  }

  /// @brief Unifies a projection expression. 
  void unifyProjectExp(ProjectExp* e) {
    ProjectExp::Kind kind = e->getKind();
    TypeVar* dataTVar = tc.getFreshTypeVar();
    
    // unify the base expression
    expectTypeToBe(e->getBase(), kind == ProjectExp::DOT ?
      static_cast<Type*>(dataTVar) : tc.getRefType(dataTVar, false));
    
    // lookup the data type decl from the inferred type of base
    Type* dataType = tvarBindings.lookup(find(dataTVar));
    NameType* nameType = dataType ? NameType::downcast(dataType) : nullptr;
    if (nameType == nullptr) {
      errors.push_back(LocatedError()
        << "Could not infer what data type is being indexed.\n"
        << e->getBase()->getLocation()
      );
      e->setType(tc.getFreshTypeVar());
      return;
    }
    StructDecl* dd = ont.getType(nameType->asString);
    
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
      e->setType(tc.getFreshTypeVar());
      return;
    }
    Type* fieldTy = tc.getTypeFromTypeExp(fieldTExp);

    // set the type of e
    e->setType(kind==ProjectExp::BRACKETS ? tc.getRefType(fieldTy) : fieldTy);
  }

private:

  /// @brief Binds type variables and narrows type constraints to make @p ty1
  /// and @p ty2 resolve to the exact same type. This is basically the "union"
  /// part of union-find.
  ///
  /// This is an annoying function from a code-maintenance perspective because
  /// the _pair_ of types must be case-matched. To make the exhaustiveness of
  /// the match visually obvious, each branch is a one-line `return nullptr` or
  /// `return unifyH(...)`. This also nicely subdivides the work into maneagable
  /// unifyH() functions with disjoint parameter signatures.
  ///
  /// @return the type that both types resolve to after unification, or nullptr
  /// if unification is not possible.
  /// @note Currently state could possibly be modified even if unification
  /// fails, which is not desirable.
  Type* unify(Type* ty1, Type* ty2) {
    if (auto t1 = Constraint::downcast(ty1)) {
      if (auto t2 = Constraint::downcast(ty2))      return unifyH(t1, t2);
      if (auto t2 = NameType::downcast(ty2))        return nullptr;
      if (auto t2 = PrimitiveType::downcast(ty2))   return unifyH(t1, t2);
      if (auto t2 = RefType::downcast(ty2))         return nullptr;
      if (auto t2 = TypeVar::downcast(ty2))         return unifyH(t2, t1);
    }
    if (auto t1 = NameType::downcast(ty1)) {
      if (auto t2 = Constraint::downcast(ty2))      return nullptr;
      if (auto t2 = NameType::downcast(ty2))        return unifyH(t1, t2);
      if (auto t2 = PrimitiveType::downcast(ty2))   return nullptr;
      if (auto t2 = RefType::downcast(ty2))         return nullptr;
      if (auto t2 = TypeVar::downcast(ty2))         return unifyH(t2, t1);
    }
    if (auto t1 = PrimitiveType::downcast(ty1)) {
      if (auto t2 = Constraint::downcast(ty2))      return unifyH(t2, t1);
      if (auto t2 = NameType::downcast(ty2))        return nullptr;
      if (auto t2 = PrimitiveType::downcast(ty2))   return unifyH(t1, t2);
      if (auto t2 = RefType::downcast(ty2))         return nullptr;
      if (auto t2 = TypeVar::downcast(ty2))         return unifyH(t2, t1);
    }
    if (auto t1 = RefType::downcast(ty1)) {
      if (auto t2 = Constraint::downcast(ty2))      return nullptr;
      if (auto t2 = NameType::downcast(ty2))        return nullptr;
      if (auto t2 = PrimitiveType::downcast(ty2))   return nullptr;
      if (auto t2 = RefType::downcast(ty2))         return unifyH(t1, t2);
      if (auto t2 = TypeVar::downcast(ty2))         return unifyH(t2, t1);
    }
    if (auto t1 = TypeVar::downcast(ty1)) {
      if (auto t2 = TypeVar::downcast(ty2))         return unifyH(t1, t2);
      else                                          return unifyH(t1, ty2);
    }
    llvm_unreachable("Unifier::unify(Type*, Type*) unrecognized case");
  }

  Type* unifyH(Constraint* c1, Constraint* c2) {
    return c1->kind == Constraint::DECIMAL || c2->kind == Constraint::DECIMAL ?
           tc.getDecimal() :
           tc.getNumeric();
  }

  Type* unifyH(Constraint* c, PrimitiveType* p) {
    if (c->kind == Constraint::DECIMAL) {
      switch (p->kind) {
      case PrimitiveType::f32:
      case PrimitiveType::f64: return p;
      default: return nullptr;
      }
    }
    if (c->kind == Constraint::NUMERIC) {
      switch (p->kind) {
      case PrimitiveType::f32:
      case PrimitiveType::f64:
      case PrimitiveType::i8:
      case PrimitiveType::i16:
      case PrimitiveType::i32:
      case PrimitiveType::i64: return p;
      default: return nullptr;
      }
    }
    llvm_unreachable("Unexpected case");
  }

  Type* unifyH(PrimitiveType* p1, PrimitiveType* p2)
    { return p1 == p2 ? p1 : nullptr; }

  Type* unifyH(NameType* n1, NameType* n2)
    { return n1 == n2 ? n1 : nullptr; }

  Type* unifyH(RefType* r1, RefType* r2) {
    if (r1->unique != r2->unique) return nullptr;
    Type* unifiedInner = unify(r1->inner, r2->inner);
    if (unifiedInner == nullptr) return nullptr;
    return tc.getRefType(unifiedInner, r1->unique);
  }

  Type* unifyH(TypeVar* v1, TypeVar* v2) {
    TypeVar* w1 = find(v1);
    TypeVar* w2 = find(v2);
    if (w1 == w2) return w1;
    Type* t1 = tvarBindings.lookup(w1);
    Type* t2 = tvarBindings.lookup(w2);
    if (t2 == nullptr) {
      tvarEquiv[w2] = w1;
      return w1;
    } else if (t1 == nullptr) {
      tvarEquiv[w1] = w2;
      return w2;
    } else {
      Type* unifiedType = unify(t1, t2);
      tvarEquiv[w2] = w1;
      tvarBindings.erase(w2);
      tvarBindings[w1] = unifiedType;
      return unifiedType;
    }
  }

  /// @note @p t cannot be a TypeVar.
  Type* unifyH(TypeVar* v, Type* t) {
    assert(TypeVar::downcast(t) == nullptr && "Wrong function!");
    TypeVar* vr = find(v);
    if (Type* vt = tvarBindings.lookup(vr)) {
      Type* unifiedType = unify(vt, t);
      if (unifiedType) { tvarBindings[vr] = unifiedType; }
      return unifiedType;
    } else {
      bind(vr, t);
      return t;
    }
  }

  /// @brief Unifies @p exp, then unifies the inferred type with @p expectedTy.
  /// An error message is pushed if the second unification fails.
  /// @return The inferred type of @p exp (for convenience).
  Type* expectTypeToBe(Exp* exp, Type* expectedTy) {
    Type* inferredTy = unifyExp(exp);
    if (unify(inferredTy, expectedTy)) return inferredTy;
    errors.push_back(LocatedError()
      << "Inferred type is " << softResolveType(inferredTy)->asString()
      << " but expected type " << softResolveType(expectedTy)->asString()
      << ".\n" << exp->getLocation()
    );
    return inferredTy;
  }

  /// @brief Removes as many type variables as possible from @p ty.
  Type* softResolveType(Type* ty) {
    if (auto v = TypeVar::downcast(ty)) {
      TypeVar* w = find(v);
      Type* t = tvarBindings.lookup(w);
      return t ? t : w;
    }
    if (auto refTy = RefType::downcast(ty)) {
      return tc.getRefType(softResolveType(refTy->inner), refTy->unique);
    }
    if (Constraint::downcast(ty)) return ty;
    if (NameType::downcast(ty)) return ty;
    if (PrimitiveType::downcast(ty)) return ty;
    llvm_unreachable("Unifier::softResolveType() unexpected case");
  }

  /// @brief Returns the representative value of the equivalence class
  /// containing @p v (i.e., the `find` operation of union-find).
  TypeVar* find(TypeVar* v) {
    while (auto v1 = tvarEquiv.lookup(v)) v = v1;
    return v;
  }

  /// @brief Binds type variable @p v to type @p t.
  /// @param t must _not_ be a type variable.
  void bind(TypeVar* v, Type* t) {
    assert(TypeVar::downcast(t) == nullptr && "Tried to bind to a tvar");
    tvarBindings[v] = t;
  }

  /// @brief Adds each parameter and its type to `localVarTypes`. 
  void addParamsToLocalVarTypes(ParamList* paramList) {
    for (auto param : paramList->asArrayRef()) {
      std::string paramName = param.first->asStringRef().str();
      Type* paramTy = tc.getTypeFromTypeExp(param.second);
      localVarTypes.add(paramName, paramTy);
    }
  }
};

#endif