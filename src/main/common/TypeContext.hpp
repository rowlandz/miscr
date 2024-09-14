#ifndef COMMON_TYPECONTEXT
#define COMMON_TYPECONTEXT

#include "llvm/ADT/DenseMap.h"
#include "common/AST.hpp"

class TypeContext {
  struct MyDenseMapInfo {
    static inline TVar getEmptyKey() { return TVar(-1); }
    static inline TVar getTombstoneKey() { return TVar(-2); }
    static unsigned getHashValue(const TVar &Val) { return Val.get(); }
    static bool isEqual(const TVar &LHS, const TVar &RHS) {
      return LHS.get() == RHS.get();
    }
  };

  /// Type variable bindings.
  llvm::DenseMap<TVar, TypeOrTVar, MyDenseMapInfo> bindings;

  int firstUnusedTypeVar = 1;

public:

  TypeContext() {}

  /// @brief Returns a fresh type variable bound to `ty`.
  TVar fresh(Type ty) {
    bindings[TVar(firstUnusedTypeVar)] = TypeOrTVar(ty);
    return firstUnusedTypeVar++;
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

  std::string TVarToString(TVar v) const {
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
};

#endif