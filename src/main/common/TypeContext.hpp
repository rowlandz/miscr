#ifndef COMMON_TYPECONTEXT
#define COMMON_TYPECONTEXT

#include "llvm/ADT/DenseMap.h"
#include "common/AST.hpp"

/// @brief Manages type variables and their bindings, including generating fresh
/// type vars, binding them to other type vars, resolving them to types, and
/// printing types.
class TypeContext {
  
  struct MyDenseMapInfo {
    static inline TVar getEmptyKey() { return TVar(-1); }
    static inline TVar getTombstoneKey() { return TVar(-2); }
    static unsigned getHashValue(const TVar &Val) { return Val.get(); }
    static bool isEqual(const TVar &LHS, const TVar &RHS) {
      return LHS.get() == RHS.get();
    }
  };

  class TypeOrTVar {
    bool isType_;
    union { TVar tvar; Type type; } data;
  public:
    TypeOrTVar() : data({.type = Type::notype()}) { isType_ = true; }
    TypeOrTVar(TVar tvar) : data({.tvar = tvar}) { isType_ = false; }
    TypeOrTVar(Type type) : data({.type = type}) { isType_ = true; }
    bool isType() { return isType_; }
    bool isTVar() { return !isType_; }
    Type getType() { return data.type; }
    TVar getTVar() { return data.tvar; }
    bool exists() {
      return (isType_ && !data.type.isNoType())
          || (!isType_ && data.tvar.exists());
    }
  };

  /// Type variable bindings.
  llvm::DenseMap<TVar, TypeOrTVar, MyDenseMapInfo> bindings;

  int firstUnusedTypeVar = 1;

public:

  /// @brief Returns a fresh type variable bound to `ty`.
  TVar fresh(Type ty) {
    bindings[TVar(firstUnusedTypeVar)] = TypeOrTVar(ty);
    return firstUnusedTypeVar++;
  }

  /// @brief Returns a fresh unbound type variable. 
  TVar fresh() {
    return TVar(firstUnusedTypeVar++);
  }

  /// @brief Binds type variable `w` to `bindTo`. If `w` is already bound to
  /// something, the binding is overwritten.
  void bind(TVar w, TVar bindTo) {
    bindings[w] = TypeOrTVar(bindTo);
  }

  /// @brief Follows `bindings` until a type var is found that is either bound
  /// to a type or bound do nothing. In the latter case, `Type::notype()` is
  /// returned. Side-effect free. 
  std::pair<TVar, Type> resolve(TVar v) const {
    assert(v.exists() && "Tried to resolve a nonexistant typevar");
    TypeOrTVar found = bindings.lookup(v);
    while (found.exists()) {
      if (found.isType()) {
        return std::pair<TVar, Type>(v, found.getType());
      }
      v = found.getTVar();
      found = bindings.lookup(v);
    }
    return std::pair<TVar, Type>(v, Type::notype());
  }

  /// @brief Converts a type expression into a `Type` with fresh type variables.
  TVar freshFromTypeExp(TypeExp* _texp) {
    AST::ID id = _texp->getID();

    if (auto texp = NameTypeExp::downcast(_texp)) {
      return fresh(Type::name(texp->getName()));
    }
    if (auto texp = RefTypeExp::downcast(_texp)) {
      if (texp->isOwned())
        return fresh(Type::oref(freshFromTypeExp(texp->getPointeeType())));
      else
        return fresh(Type::bref(freshFromTypeExp(texp->getPointeeType())));
    }
    if (id == AST::ID::BOOL_TEXP) return fresh(Type::bool_());
    if (id == AST::ID::f32_TEXP) return fresh(Type::f32());
    if (id == AST::ID::f64_TEXP) return fresh(Type::f64());
    if (id == AST::ID::i8_TEXP) return fresh(Type::i8());
    if (id == AST::ID::i16_TEXP) return fresh(Type::i16());
    if (id == AST::ID::i32_TEXP) return fresh(Type::i32());
    if (id == AST::ID::i64_TEXP) return fresh(Type::i64());
    if (id == AST::ID::UNIT_TEXP) return fresh(Type::unit());
    llvm_unreachable("Ahhh!!!");
  }

  std::string TVarToString(TVar v) const {
    if (!v.exists()) return std::string("__nonexistant_tvar__");
    auto res = resolve(v);
    if (res.second.isNoType()) {
      return std::string("?") + std::to_string(v.get());
    } else {
      switch (res.second.getID()) {
      case Type::ID::BREF:
        return std::string("bref<") + TVarToString(res.second.getInner()) + ">";
      case Type::ID::OREF:
        return std::string("oref<") + TVarToString(res.second.getInner()) + ">";
      case Type::ID::BOOL: return std::string("bool");
      case Type::ID::DECIMAL: return std::string("decimal");
      case Type::ID::f32: return std::string("f32");
      case Type::ID::f64: return std::string("f64");
      case Type::ID::i8: return std::string("i8");
      case Type::ID::i16: return std::string("i16");
      case Type::ID::i32: return std::string("i32");
      case Type::ID::i64: return std::string("i64");
      case Type::ID::NAME: return res.second.getName()->asStringRef().str();
      case Type::ID::NUMERIC: return std::string("numeric");
      case Type::ID::UNIT: return std::string("unit");
      default: return std::string("???");
      }
    }
  }
};

#endif