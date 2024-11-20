#ifndef COMMON_TYPECONTEXT
#define COMMON_TYPECONTEXT

#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/StringMap.h>
#include "common/AST.hpp"

/// @brief Manages creation, deletion, and uniquing of Type objects.
///
/// Types are created using `get` methods and are destroyed by the TypeContext
/// destructor or the clear() method.
class TypeContext {

  // primitive types
  PrimitiveType bool_ = PrimitiveType::BOOL;
  PrimitiveType f32   = PrimitiveType::f32;
  PrimitiveType f64   = PrimitiveType::f64;
  PrimitiveType i8    = PrimitiveType::i8;
  PrimitiveType i16   = PrimitiveType::i16;
  PrimitiveType i32   = PrimitiveType::i32;
  PrimitiveType i64   = PrimitiveType::i64;
  PrimitiveType unit  = PrimitiveType::UNIT;

  // type constraints
  Constraint decimal  = Constraint::DECIMAL;
  Constraint numeric  = Constraint::NUMERIC;

  /// @brief Counter for generating fresh type variables.
  int firstUnusedTypeVar = 1;

  /// @brief Stores all borrowed reference types, indexed by their inner type.
  llvm::DenseMap<Type*, RefType*> brefTypes;

  /// @brief Stores all owned reference types, indexed by their inner type.
  llvm::DenseMap<Type*, RefType*> orefTypes;

  /// @brief Stores all NameType objects indexed by their names.
  llvm::StringMap<NameType*> nameTypes;

  /// @brief Stores all TypeVar objects.
  llvm::SmallVector<TypeVar*> typeVars;

public:
  TypeContext() {}
  ~TypeContext() { clear(); }
  TypeContext(const TypeContext&) = delete;
  TypeContext& operator=(const TypeContext&) = delete;

  PrimitiveType* getBool() { return &bool_; }
  PrimitiveType* getF32() { return &f32; }
  PrimitiveType* getF64() { return &f64; }
  PrimitiveType* getI8() { return &i8; }
  PrimitiveType* getI16() { return &i16; }
  PrimitiveType* getI32() { return &i32; }
  PrimitiveType* getI64() { return &i64; }
  PrimitiveType* getUnit() { return &unit; }

  Constraint* getDecimal() { return &decimal; }
  Constraint* getNumeric() { return &numeric; }

  RefType* getBrefType(Type* inner) {
    if (RefType* ret = brefTypes.lookup(inner)) return ret;
    RefType* ret = new RefType(inner, false);
    brefTypes[inner] = ret;
    return ret;
  }

  RefType* getOrefType(Type* inner) {
    if (RefType* ret = orefTypes.lookup(inner)) return ret;
    RefType* ret = new RefType(inner, true);
    orefTypes[inner] = ret;
    return ret;
  }

  RefType* getRefType(Type* inner, bool isOwned)
    { return isOwned ? getOrefType(inner) : getBrefType(inner); }

  NameType* getNameType(llvm::StringRef name) {
    if (NameType* ret = nameTypes.lookup(name)) return ret;
    NameType* ret = new NameType(name);
    nameTypes[name] = ret;
    return ret;
  }

  /// @brief Returns a fresh type variable that has never been built before.
  TypeVar* getFreshTypeVar() {
    TypeVar* ret = new TypeVar(firstUnusedTypeVar++);
    typeVars.push_back(ret);
    return ret;
  }

  /// @brief Gets the type corresponding to the given type expression.
  Type* getTypeFromTypeExp(TypeExp* texp) {
    if (auto nte = NameTypeExp::downcast(texp)) {
      return getNameType(nte->getName()->asStringRef());
    }
    if (auto rte = RefTypeExp::downcast(texp)) {
      if (rte->isOwned())
        return getOrefType(getTypeFromTypeExp(rte->getPointeeType()));
      else
        return getBrefType(getTypeFromTypeExp(rte->getPointeeType()));
    }
    if (auto pte = PrimitiveTypeExp::downcast(texp)) {
      switch (pte->kind) {
      case PrimitiveTypeExp::BOOL:   return &bool_;
      case PrimitiveTypeExp::f32:    return &f32;
      case PrimitiveTypeExp::f64:    return &f64;
      case PrimitiveTypeExp::i8:     return &i8;
      case PrimitiveTypeExp::i16:    return &i16;
      case PrimitiveTypeExp::i32:    return &i32;
      case PrimitiveTypeExp::i64:    return &i64;
      case PrimitiveTypeExp::UNIT:   return &unit;
      }
    }
    llvm_unreachable("TypeContext::getTypeFromTypeExp() unexpected case");
  }

  /// @brief Factory-resets this TypeContext. All Type objects created by this
  /// TypeContext are deleted.
  void clear() {
    for (auto ty : brefTypes) { delete ty.second; }
    for (auto ty : orefTypes) { delete ty.second; }
    for (auto name : nameTypes.keys()) { delete nameTypes[name]; }
    for (auto ty : typeVars) { delete ty; }
    brefTypes.clear();
    orefTypes.clear();
    nameTypes.clear();
    typeVars.clear();
  }
};

#endif