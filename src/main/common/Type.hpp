#ifndef COMMON_TYPE
#define COMMON_TYPE

#include <string>
#include <llvm/ADT/StringRef.h>

class TypeContext;

/// @brief The base class of all MiSCR types and type constraints.
///
/// Not to be confused with TypeExp which are syntax elements, Type is the
/// representation of MiSCR types used by the type checker.
///
/// Types are structurally uniqued and thus must be created by a TypeContext.
///
/// All Type objects are immutable after creation. Public fields are always
/// `const` and methods never mutate information. This effectively makes every
/// `Type*` a `const Type*`, but `const` is always omitted to save keystrokes.
class Type {
public:
  enum class ID : unsigned char { CONSTRAINT, NAME, PRIMITIVE, REF, VAR };

  /// @brief What kind of type this is.
  const ID id;

  /// @brief Returns a string representation. For concrete types, the string
  /// will match the concrete syntax.
  std::string asString();

protected:
  Type(ID id) : id(id) {}
  ~Type() {}
};

/// @brief A primitive concrete type.
class PrimitiveType : public Type {
  friend class TypeContext;

public:
  enum Kind : unsigned char { BOOL, f32, f64, i8, i16, i32, i64, UNIT };

  /// @brief The kind of primitive type this is.
  const Kind kind;

  static PrimitiveType* downcast(Type* ty)
    {return ty->id==ID::PRIMITIVE ? static_cast<PrimitiveType*>(ty) : nullptr;}

private:
  PrimitiveType(Kind kind) : Type(ID::PRIMITIVE), kind(kind) {}
  ~PrimitiveType() {}
};

/// @brief A type constraint (e.g., a set of concrete types).
class Constraint : public Type {
  friend class TypeContext;

public:
  enum Kind : unsigned char { DECIMAL, NUMERIC };

  /// @brief The kind of type constraint this is.
  const Kind kind;

  static Constraint* downcast(Type* ty)
    {return ty->id == ID::CONSTRAINT ? static_cast<Constraint*>(ty) : nullptr;}

private:
  Constraint(Kind kind) : Type(ID::CONSTRAINT), kind(kind) {}
  ~Constraint() {}
};

/// @brief An owned or borrowed reference type.
class RefType : public Type {
  friend class TypeContext;
public:

  /// @brief True iff this is an owned reference.
  const bool isOwned;

  /// @brief Reference that points to _what_.
  Type* const inner;

  static RefType* downcast(Type* ty)
    { return ty->id == ID::REF ? static_cast<RefType*>(ty) : nullptr; }

private:
  RefType(Type* inner, bool isOwned)
    : Type(ID::REF), isOwned(isOwned), inner(inner) {}
  ~RefType() {}
};

/// @brief A user-defined data type.
class NameType : public Type {
  friend class TypeContext;
public:

  const std::string asString;

  static NameType* downcast(Type* ty)
    { return ty->id == ID::NAME ? static_cast<NameType*>(ty) : nullptr; }

private:
  NameType(llvm::StringRef s) : Type(ID::NAME), asString(s) {}
  ~NameType() {}
};

/// @brief A type variable.
class TypeVar : public Type {
  friend class TypeContext;
  TypeVar(int id) : Type(ID::VAR), id(id) {}
  ~TypeVar() {}
public:
  const int id;
  static TypeVar* downcast(Type* ty)
    { return ty->id == ID::VAR ? static_cast<TypeVar*>(ty) : nullptr; }
};

//============================================================================//

std::string Type::asString() {
  if (auto ty = Constraint::downcast(this)) {
    switch (ty->kind) {
    case Constraint::DECIMAL:   return "decimal";
    case Constraint::NUMERIC:   return "numeric";
    }
  }
  if (auto ty = NameType::downcast(this)) {
    return ty->asString;
  }
  if (auto ty = PrimitiveType::downcast(this)) {
    switch (ty->kind) {
    case PrimitiveType::BOOL:   return "bool";
    case PrimitiveType::f32:    return "f32";
    case PrimitiveType::f64:    return "f64";
    case PrimitiveType::i8:     return "i8";
    case PrimitiveType::i16:    return "i16";
    case PrimitiveType::i32:    return "i32";
    case PrimitiveType::i64:    return "i64";
    case PrimitiveType::UNIT:   return "unit";
    }
  }
  if (auto ty = RefType::downcast(this)) {
    if (ty->isOwned)
      return "#" + ty->inner->asString();
    else
      return "&" + ty->inner->asString();
  }
  if (auto ty = TypeVar::downcast(this)) {
    return "$var" + std::to_string(ty->id);
  }
  llvm_unreachable("Type::asString() unexpected case");
}

#endif