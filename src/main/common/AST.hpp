#ifndef COMMON_AST
#define COMMON_AST

#include <cstdio>
#include <string>
#include "common/ASTContext.hpp"
#include "common/Location.hpp"

class Exp;
class IntLit;

/// @brief An AST node is any syntactic form that appears in source code. All
/// AST nodes have an `id`, which is used to downcast to subclasses, and a
/// source code `location`. AST nodes may contain pseudo-pointers (instances
/// of `Addr`) to other nodes. An `ASTContext` manages these nodes including,
/// creation and dereferencing.
///
/// All subclasses must fit within 32 bytes (including the 10 taken up by the
/// location and ID).
class AST {
public:
  enum ID : unsigned short {
    // expressions and statements
    ADD, ARRAY_INIT, ARRAY_LIST, ASCRIP, BLOCK, CALL, DEC_LIT, DEREF, DIV,
    ENAME, EQ, FALSE, GE, GT, IF, INDEX, INT_LIT, LE, LET, LT, MUL, NE, REF_EXP,
    RETURN, STORE, STRING_LIT, SUB, TRUE, WREF_EXP,

    // declarations
    EXTERN_FUNC, FUNC, MODULE, NAMESPACE,

    // type expressions
    ARRAY_TEXP, BOOL_TEXP, f32_TEXP, f64_TEXP, i8_TEXP, i16_TEXP, i32_TEXP,
    i64_TEXP, REF_TEXP, STR_TEXP, UNIT_TEXP, WREF_TEXP, 

    // other
    DECLLIST_CONS, DECLLIST_NIL, EXPLIST_CONS, EXPLIST_NIL, FQIDENT, IDENT,
    PARAMLIST_CONS, PARAMLIST_NIL, QIDENT,
  };

private:
  Location location;
  ID id;

protected:
  AST(ID id, Location location) : id(id), location(location) {}

public:
  ID getID() const { return id; }
  Location getLocation() const { return location; }

  bool isExp() const {
    switch (id) {
    case ADD: case ARRAY_INIT: case ARRAY_LIST: case ASCRIP: case BLOCK:
    case CALL: case DEC_LIT: case DEREF: case DIV: case ENAME: case EQ:
    case FALSE: case GE: case GT: case IF: case INDEX: case INT_LIT: case LE:
    case LET: case LT: case MUL: case NE: case REF_EXP: case RETURN: case STORE:
    case STRING_LIT: case SUB: case TRUE: case WREF_EXP:
      return true;
    default: return false;
    }
  }

  bool isBinopExp() const {
    switch (id) {
    case ADD: case DIV: case EQ: case GE: case GT: case LE: case LT: case MUL:
    case NE: case SUB: return true;
    default: return false;
    }
  }
};

/// TODO: can we include the pointer to the ontology in this struct? 
class FQNameKey {
  unsigned int value;
public:
  FQNameKey() { value = -1; }
  FQNameKey(unsigned int value) { this->value = value; }
  unsigned int getValue() const { return value; }
};

/// @brief A type variable that annotates an expression. Type variables are
/// managed by a `TypeContext` which resolves them to `Type` instances.
class TVar {
  int value;
public:
  static TVar none() { return TVar(-1); }
  TVar() { this->value = -1; }
  TVar(int value) { this->value = value; }
  int get() const { return value; }
  bool exists() { return value >= 0; }
};

/// @brief A concrete type (e.g., `i32`, `bool`) or a type constraint (e.g.,
/// `numeric`).
class Type {
public:
  enum struct ID : unsigned char {
    // concrete types
    ARRAY_SART,  // array (sized at runtime)
    ARRAY_SACT,  // array (sized at compile-time)
    BOOL, f32, f64, i8, i16, i32, i64, RREF, UNIT, WREF,

    // type constraints
    DECIMAL, NUMERIC, REF,

    // other
    NOTYPE,
  };
private:
  Type::ID id;
  union { unsigned int compileTime; Addr<Exp> runtime; } arraySize;
  TVar inner;
  Type(Type::ID id) : arraySize({.compileTime=0}) { this->id = id; }
  Type(Type::ID id, TVar inner) : arraySize({.compileTime=0}) {
    this->id = id; this->inner = inner;
  }
  Type(Addr<Exp> arraySize, TVar of) : arraySize({.runtime = arraySize}) {
    this->id = ID::ARRAY_SART; this->inner = of;
  }
  Type(unsigned int arrSize, TVar of) : arraySize({.compileTime = arrSize}) {
    this->id = ID::ARRAY_SACT; this->inner = of;
  }
public:
  /// @brief Construct an array type sized at runtime. 
  static Type array_sart(Addr<Exp> sz, TVar of) { return Type(sz, of); }
  static Type array_sact(unsigned int sz, TVar of) { return Type(sz, of); }
  static Type bool_() { return Type(ID::BOOL); }
  static Type f32() { return Type(ID::f32); }
  static Type f64() { return Type(ID::f64); }
  static Type i8() { return Type(ID::i8); }
  static Type i16() { return Type(ID::i16); }
  static Type i32() { return Type(ID::i32); }
  static Type i64() { return Type(ID::i64); }
  static Type ref(TVar of) { return Type(ID::REF, of); }
  static Type rref(TVar of) { return Type(ID::RREF, of); }
  static Type unit() { return Type(ID::UNIT); }
  static Type wref(TVar of) { return Type(ID::WREF, of); }
  static Type decimal() { return Type(ID::DECIMAL); }
  static Type numeric() { return Type(ID::NUMERIC); }
  static Type notype() { return Type(ID::NOTYPE); }
  bool isNoType() const { return id == ID::NOTYPE; }
  bool isArrayType() const {
    return id == ID::ARRAY_SACT || id == ID::ARRAY_SART;
  }
  ID getID() const { return id; }
  TVar getInner() const { return inner; }
  unsigned int getCompileTimeArraySize() const { return arraySize.compileTime; }
  Addr<Exp> getRuntimeArraySize() const { return arraySize.runtime; }
};

/// @brief A qualified, unqualified, or fully qualified identifier
/// (QIDENT, IDENT, FQIDENT).
class Name : public AST {
protected:
  Name(ID id, Location loc) : AST(id, loc) {}
public:
  bool isQualified() { return getID() == QIDENT; }
  bool isUnqualified() { return getID() == IDENT; }
};

/// @brief An unqualified name.
class Ident : public Name {
  const char* ptr;
public:
  Ident(Location loc, const char* ptr) : Name(IDENT, loc) {
    this->ptr = ptr;
  }
  std::string asString() { return std::string(ptr, getLocation().sz); }
};

/// @brief A name with at least one qualifier (the head).
class QIdent : public Name {
  Addr<Ident> head;
  Addr<Name> tail;
public:
  QIdent(Location loc, Addr<Ident> head, Addr<Name> tail) : Name(QIDENT, loc) {
    this->head = head; this->tail = tail;
  }
  Addr<Ident> getHead() const { return head; }
  Addr<Name> getTail() const { return tail; }
};

/// @brief A fully qualified name.
class FQIdent : public Name {
  FQNameKey key;
public:
  FQIdent(Location loc, FQNameKey key) : Name(FQIDENT, loc), key(key) {}
  FQNameKey getKey() { return key; }
};

/// @brief A type that appears in the source text.
class TypeExp : public AST {
protected:
  TypeExp(ID id, Location loc) : AST(id, loc) {}
public:
  static TypeExp mkUnit(Location loc) { return TypeExp(UNIT_TEXP, loc); }
  static TypeExp mkBool(Location loc) { return TypeExp(BOOL_TEXP, loc); }
  static TypeExp mkI8(Location loc) { return TypeExp(i8_TEXP, loc); }
  static TypeExp mkI16(Location loc) { return TypeExp(i16_TEXP, loc); }
  static TypeExp mkI32(Location loc) { return TypeExp(i32_TEXP, loc); }
  static TypeExp mkI64(Location loc) { return TypeExp(i64_TEXP, loc); }
  static TypeExp mkF32(Location loc) { return TypeExp(f32_TEXP, loc); }
  static TypeExp mkF64(Location loc) { return TypeExp(f64_TEXP, loc); }
  static TypeExp mkStr(Location loc) { return TypeExp(STR_TEXP, loc); }
};

/// @brief A read-only or writable reference type expression.
class RefTypeExp : public TypeExp {
  Addr<TypeExp> pointeeType;
  bool writable;
public:
  RefTypeExp(Location loc, Addr<TypeExp> pointeeType, bool writable)
      : TypeExp(writable ? WREF_TEXP : REF_TEXP, loc) {
    this->pointeeType = pointeeType;
    this->writable = writable;
  }
  Addr<TypeExp> getPointeeType() const { return pointeeType; }
  bool isWritable() const { return writable; }
};

/// @brief Type expression of a constant-size array `[20 of i32]` or
/// unknown-size array `[n of i32]` or `[_ of i32]`.
class ArrayTypeExp : public TypeExp {
  Addr<Exp> size;
  Addr<TypeExp> innerType;
public:
  /// @brief If `size` is an error address, then an underscore was used to
  /// indicate the array size. 
  ArrayTypeExp(Location loc, Addr<Exp> size, Addr<TypeExp> innerType)
      : TypeExp(ARRAY_TEXP, loc) {
    this->size = size; this->innerType = innerType;
  }
  /// @brief This returns an error address if size is unknown.
  Addr<Exp> getSize() const { return size; }
  Addr<TypeExp> getInnerType() const { return innerType; }
  bool hasUnderscoreSize() const { return size.isError(); }
};

/// @brief An expression or statement (currently there is no distinction).
class Exp : public AST {
protected:
  TVar type = TVar::none();
  Exp(ID id, Location loc) : AST(id, loc) {}
public:
  TVar getTVar() const { return type; };
  void setTVar(TVar type) { this->type = type; }
};

/// @brief A linked-list of expressions.
class ExpList : public AST {
  Addr<Exp> head;
  Addr<ExpList> tail;
public:
  ExpList(Location loc) : AST(EXPLIST_NIL, loc) {
    this->head = Addr<Exp>::none();
    this->tail = Addr<ExpList>::none();
  }
  ExpList(Location loc, Addr<Exp> head, Addr<ExpList> tail)
      : AST(EXPLIST_CONS, loc) {
    this->head = head;
    this->tail = tail;
  }
  bool isEmpty() const { return getID() == EXPLIST_NIL; }
  bool nonEmpty() const { return getID() == EXPLIST_CONS; }

  /// Returns the first expression in this list. This is only safe to call
  /// if `isEmpty` returns false.
  Addr<Exp> getHead() const { return head; }

  /// Returns the tail of this expression list. This is only safe to call
  /// if `isEmpty` returns false;
  Addr<ExpList> getTail() const { return tail; }
};

/// @brief A list of parameters in a function signature.
class ParamList : public AST {
  Addr<Ident> headParamName;
  Addr<TypeExp> headParamType;
  Addr<ParamList> tail;
public:
  ParamList(Location loc) : AST(PARAMLIST_NIL, loc) {
    this->headParamName = Addr<Ident>::none();
    this->headParamType = Addr<TypeExp>::none();
    this->tail = Addr<ParamList>::none();
  }
  ParamList(Location loc, Addr<Ident> headParamName,
      Addr<TypeExp> headParamType, Addr<ParamList> tail)
      : AST(PARAMLIST_CONS, loc) {
    this->headParamName = headParamName;
    this->headParamType = headParamType;
    this->tail = tail;
  }
  bool isEmpty() { return getID() == PARAMLIST_NIL; }
  bool nonEmpty() { return getID() == PARAMLIST_CONS; }
  Addr<Ident> getHeadParamName() const { return headParamName; }
  Addr<TypeExp> getHeadParamType() const { return headParamType; }
  Addr<ParamList> getTail() const { return tail; }
};

/// @brief A boolean literal (`true` or `false`).
class BoolLit : public Exp {
  bool value;
public:
  BoolLit(Location loc, bool value) : Exp(value ? TRUE : FALSE, loc) {
    this->value = value;
  }
  bool getValue() const { return value; }
};

/// @brief An integer literal.
class IntLit : public Exp {
  const char* ptr;
public:
  IntLit(Location loc, const char* ptr) : Exp(INT_LIT, loc) {
    this->ptr = ptr;
  }
  long asLong() const {
    return atol(std::string(ptr, getLocation().sz).c_str());
  }
  unsigned int asUint() const {
    return atoi(std::string(ptr, getLocation().sz).c_str());
  }
  std::string asString() { return std::string(ptr, getLocation().sz); }
};

/// @brief A decimal literal.
class DecimalLit : public Exp {
  const char* ptr;
public:
  DecimalLit(Location loc, const char* ptr) : Exp(DEC_LIT, loc) {
    this->ptr = ptr;
  }
  std::string asString() const { return std::string(ptr, getLocation().sz); }
  double asDouble() const {
    return atof(std::string(ptr, getLocation().sz).c_str());
  }
};

/// @brief A string literal.
class StringLit : public Exp {
  const char* ptr;
public:
  /// Creates a string literal. The `loc` and `ptr` should point to the open
  /// quote of the string literal. 
  StringLit(Location loc, const char* ptr) : Exp(STRING_LIT, loc) {
    this->ptr = ptr;
  }
  /// Returns the contents of the string (excluding surrounding quotes).
  std::string asString() const {
    return std::string(ptr+1, getLocation().sz-2);
  }

  /// Returns the contents of the string with escape sequences processed. 
  std::string processEscapes() const {
    std::string ret;
    ret.reserve(getLocation().sz);
    const char* const end = ptr + getLocation().sz - 1;
    for (const char* p = ptr + 1; p < end; ++p) {
      if (*p == '\\') {
        switch (*(++p)) {
        case 'n': ret.push_back('\n'); break;
        case 't': ret.push_back('\t'); break;
        default: ret.push_back(*p);
        }
      } else ret.push_back(*p);
    }
    return ret;
  }
};

/// @brief A name used as an expression.
class NameExp : public Exp {
  Addr<Name> name;
public:
  NameExp(Location loc, Addr<Name> name) : Exp(ENAME, loc) {
    this->name = name;
  }
  Addr<Name> getName() const { return name; }
};

/// @brief A binary operator expression.
class BinopExp : public Exp {
  Addr<Exp> lhs;
  Addr<Exp> rhs;
public:
  BinopExp(ID id, Location loc, Addr<Exp> lhs, Addr<Exp> rhs) : Exp(id, loc) {
    this->lhs = lhs;
    this->rhs = rhs;
  }
  Addr<Exp> getLHS() const { return lhs; }
  Addr<Exp> getRHS() const { return rhs; }
};

/// @brief An if-then-else expression.
class IfExp : public Exp {
  Addr<Exp> condExp;
  Addr<Exp> thenExp;
  Addr<Exp> elseExp;
public:
  IfExp(Location loc, Addr<Exp> condExp, Addr<Exp> thenExp, Addr<Exp> elseExp)
      : Exp(IF, loc) {
    this->condExp = condExp;
    this->thenExp = thenExp;
    this->elseExp = elseExp;
  }
  Addr<Exp> getCondExp() const { return condExp; }
  Addr<Exp> getThenExp() const { return thenExp; }
  Addr<Exp> getElseExp() const { return elseExp; }
};

/// @brief A block expression.
class BlockExp : public Exp {
  Addr<ExpList> statements;
public:
  BlockExp(Location loc, Addr<ExpList> statements) : Exp(BLOCK, loc) {
    this->statements = statements;
  }
  Addr<ExpList> getStatements() const { return statements; }
};

/// @brief A function call expression.
class CallExp : public Exp {
  Addr<Name> function;
  Addr<ExpList> arguments;
public:
  CallExp(Location loc, Addr<Name> function, Addr<ExpList> arguments)
      : Exp(CALL, loc) {
    this->function = function;
    this->arguments = arguments;
  }
  Addr<Name> getFunction() const { return function; }
  Addr<ExpList> getArguments() const { return arguments; }
  void setFunction(Addr<Name> function) { this->function = function; }
};

/// @brief A type ascription expression.
class AscripExp : public Exp {
  Addr<Exp> ascriptee;
  Addr<TypeExp> ascripter;
public:
  AscripExp(Location loc, Addr<Exp> ascriptee, Addr<TypeExp> ascripter)
      : Exp(ASCRIP, loc) {
    this->ascriptee = ascriptee;
    this->ascripter = ascripter;
  }
  Addr<Exp> getAscriptee() const { return ascriptee; }
  Addr<TypeExp> getAscripter() const { return ascripter; }
};

/// @brief A let statement. The type ascription is optional.
class LetExp : public Exp {
  Addr<Ident> boundIdent;
  Addr<TypeExp> ascrip;
  Addr<Exp> definition;
public:
  LetExp(Location loc, Addr<Ident> boundIdent, Addr<TypeExp> ascrip,
      Addr<Exp> definition) : Exp(LET, loc) {
    this->boundIdent = boundIdent;
    this->ascrip = ascrip;
    this->definition = definition;
  }
  Addr<Ident> getBoundIdent() const { return boundIdent; }
  Addr<TypeExp> getAscrip() const { return ascrip; }
  Addr<Exp> getDefinition() const { return definition; }
};

/// @brief A return statement.
class ReturnExp : public Exp {
  Addr<Exp> returnee;
public:
  ReturnExp(Location loc, Addr<Exp> returnee) : Exp(RETURN, loc) {
    this->returnee = returnee;
  }
  Addr<Exp> getReturnee() const { return returnee; }
};

/// @brief A store statement (e.g., `x := 42`).
class StoreExp : public Exp {
  Addr<Exp> lhs;
  Addr<Exp> rhs;
public:
  StoreExp(Location loc, Addr<Exp> lhs, Addr<Exp> rhs) : Exp(STORE, loc) {
    this->lhs = lhs;
    this->rhs = rhs;
  }
  Addr<Exp> getLHS() const { return lhs; }
  Addr<Exp> getRHS() const { return rhs; }
};

/// @brief A read-only or writable reference constructor expression.
class RefExp : public Exp {
  Addr<Exp> initializer;
public:
  RefExp(Location loc, Addr<Exp> initializer, bool writable)
      : Exp(writable ? WREF_EXP : REF_EXP, loc) {
    this->initializer = initializer;
  }
  Addr<Exp> getInitializer() const { return initializer; }
};

/// @brief A dereference expression.
class DerefExp : public Exp {
  Addr<Exp> of;
public:
  DerefExp(Location loc, Addr<Exp> of) : Exp(DEREF, loc) { this->of = of; }
  Addr<Exp> getOf() const { return of; }
};

/// @brief An array constructor that lists out the array contents.
/// e.g., `[42, x, y+1]`
class ArrayListExp : public Exp {
  Addr<ExpList> content;
public:
  ArrayListExp(Location loc, Addr<ExpList> content) : Exp(ARRAY_LIST, loc) {
    this->content = content;
  }
  Addr<ExpList> getContent() const { return content; }
};

/// @brief An expression that constructs an array by specifying a size and
/// an element to fill all the spots. e.g., `[10 of 0]`
class ArrayInitExp : public Exp {
  Addr<Exp> size;
  Addr<Exp> initializer;
public:
  ArrayInitExp(Location loc, Addr<Exp> size, Addr<Exp> initializer)
      : Exp(ARRAY_INIT, loc) {
    this->size = size;
    this->initializer = initializer;
  }
  Addr<Exp> getSize() const { return size; }
  Addr<Exp> getInitializer() const { return initializer; }
};

/// @brief An expression that calculates an address from a base reference and
/// offset (e.g., `myarrayref[3]`).
class IndexExp : public Exp {
  Addr<Exp> base;
  Addr<Exp> index;
public:
  IndexExp(Location loc, Addr<Exp> base, Addr<Exp> index) : Exp(INDEX, loc) {
    this->base = base; this->index = index;
  }
  Addr<Exp> getBase() const { return base; }
  Addr<Exp> getIndex() const { return index; }
};

/// @brief A declaration.
class Decl : public AST {
protected:
  Addr<Name> name;
  Decl(ID id, Location loc, Addr<Name> name) : AST(id, loc) {
    this->name = name;
  }
public:
  Addr<Name> getName() const { return name; }
  void setName(Addr<FQIdent> fqName) { name = fqName.upcast<Name>(); }
};

/// @brief A list of declarations.
class DeclList : public AST {
  Addr<Decl> head;
  Addr<DeclList> tail;
public:
  DeclList(Location loc) : AST(DECLLIST_NIL, loc) {
    this->head = Addr<Decl>::none();
    this->tail = Addr<DeclList>::none();
  }
  DeclList(Location loc, Addr<Decl> head, Addr<DeclList> tail)
      : AST(DECLLIST_CONS, loc) {
    this->head = head;
    this->tail = tail;
  }
  bool isEmpty() { return getID() == DECLLIST_NIL; }
  bool nonEmpty() { return getID() == DECLLIST_CONS; }
  Addr<Decl> getHead() const { return head; }
  Addr<DeclList> getTail() const { return tail; }
};

class ModuleDecl : public Decl {
  Addr<DeclList> decls;
public:
  ModuleDecl(Location loc, Addr<Name> name, Addr<DeclList> decls)
      : Decl(MODULE, loc, name) {
    this->decls = decls;
  }
  Addr<DeclList> getDecls() const { return decls; }
};

class NamespaceDecl : public Decl {
  Addr<DeclList> decls;
public:
  NamespaceDecl(Location loc, Addr<Name> name, Addr<DeclList> decls)
      : Decl(NAMESPACE, loc, name) {
    this->decls = decls;
  }
  Addr<DeclList> getDecls() const { return decls; }
};

/// @brief A function or extern function (FUNC/EXTERN_FUNC).
class FunctionDecl : public Decl {
  Addr<ParamList> parameters;
  Addr<TypeExp> returnType;
  Addr<Exp> body;
public:
  FunctionDecl(Location loc, Addr<Name> name, Addr<ParamList> parameters,
      Addr<TypeExp> returnType, Addr<Exp> body) : Decl(FUNC, loc, name) {
    this->parameters = parameters;
    this->returnType = returnType;
    this->body = body;
  }
  FunctionDecl(Location loc, Addr<Name> name, Addr<ParamList> parameters,
      Addr<TypeExp> returnType) : Decl(EXTERN_FUNC, loc, name) {
    this->parameters = parameters;
    this->returnType = returnType;
    this->body = Addr<Exp>::none();
  }
  Addr<ParamList> getParameters() const { return parameters; }
  Addr<TypeExp> getReturnType() const { return returnType; }
  Addr<Exp> getBody() const { return body; }
};

const char* ASTIDToString(AST::ID nt) {
  switch (nt) {
  case AST::ID::ADD:                return "ADD";
  case AST::ID::ARRAY_INIT:         return "ARRAY_INIT";
  case AST::ID::ARRAY_LIST:         return "ARRAY_LIST";
  case AST::ID::ASCRIP:             return "ASCRIP";
  case AST::ID::BLOCK:              return "BLOCK";
  case AST::ID::CALL:               return "CALL";
  case AST::ID::DEC_LIT:            return "DEC_LIT";
  case AST::ID::DEREF:              return "DEREF";
  case AST::ID::DIV:                return "DIV";
  case AST::ID::ENAME:              return "ENAME";
  case AST::ID::EQ:                 return "EQ";
  case AST::ID::FALSE:              return "FALSE";
  case AST::ID::IF:                 return "IF";
  case AST::ID::GE:                 return "GE";
  case AST::ID::GT:                 return "GT";
  case AST::ID::INDEX:              return "INDEX";
  case AST::ID::INT_LIT:            return "INT_LIT";
  case AST::ID::LE:                 return "LE";
  case AST::ID::LET:                return "LET";
  case AST::ID::LT:                 return "LT";
  case AST::ID::MUL:                return "MUL";
  case AST::ID::NE:                 return "NE";
  case AST::ID::REF_EXP:            return "REF_EXP";
  case AST::ID::RETURN:             return "RETURN";
  case AST::ID::STORE:              return "STORE";
  case AST::ID::STRING_LIT:         return "STRING_LIT";
  case AST::ID::SUB:                return "SUB";
  case AST::ID::TRUE:               return "TRUE";
  case AST::ID::WREF_EXP:           return "WREF_EXP";

  case AST::ID::EXTERN_FUNC:        return "EXTERN_FUNC";
  case AST::ID::FUNC:               return "FUNC";
  case AST::ID::MODULE:             return "MODULE";
  case AST::ID::NAMESPACE:          return "NAMESPACE";

  case AST::ID::ARRAY_TEXP:         return "ARRAY_TEXP";
  case AST::ID::BOOL_TEXP:          return "BOOL_TEXP";
  case AST::ID::f32_TEXP:           return "f32_TEXP";
  case AST::ID::f64_TEXP:           return "f64_TEXP";
  case AST::ID::i8_TEXP:            return "i8_TEXP";
  case AST::ID::i16_TEXP:           return "i16_TEXP";
  case AST::ID::i32_TEXP:           return "i32_TEXP";
  case AST::ID::i64_TEXP:           return "i64_TEXP";
  case AST::ID::REF_TEXP:           return "REF_TEXP";
  case AST::ID::STR_TEXP:           return "STR_TEXP";
  case AST::ID::UNIT_TEXP:          return "UNIT_TEXP";
  case AST::ID::WREF_TEXP:          return "WREF_TEXP";
  
  case AST::ID::DECLLIST_CONS:      return "DECLLIST_CONS";
  case AST::ID::DECLLIST_NIL:       return "DECLLIST_NIL";
  case AST::ID::EXPLIST_CONS:       return "EXPLIST_CONS";
  case AST::ID::EXPLIST_NIL:        return "EXPLIST_NIL";
  case AST::ID::FQIDENT:            return "FQIDENT";
  case AST::ID::IDENT:              return "IDENT";
  case AST::ID::PARAMLIST_CONS:     return "PARAMLIST_CONS";
  case AST::ID::PARAMLIST_NIL:      return "PARAMLIST_NIL";
  case AST::ID::QIDENT:             return "QIDENT";
  }
}

AST::ID stringToASTID(const std::string& str) {
       if (str == "ADD")                 return AST::ID::ADD;
  else if (str == "ARRAY_INIT")          return AST::ID::ARRAY_INIT;
  else if (str == "ARRAY_LIST")          return AST::ID::ARRAY_LIST;
  else if (str == "ASCRIP")              return AST::ID::ASCRIP;
  else if (str == "BLOCK")               return AST::ID::BLOCK;
  else if (str == "CALL")                return AST::ID::CALL;
  else if (str == "DEC_LIT")             return AST::ID::DEC_LIT;
  else if (str == "DEREF")               return AST::ID::DEREF;
  else if (str == "DIV")                 return AST::ID::DIV;
  else if (str == "ENAME")               return AST::ID::ENAME;
  else if (str == "EQ")                  return AST::ID::EQ;
  else if (str == "FALSE")               return AST::ID::FALSE;
  else if (str == "GE")                  return AST::ID::GE;
  else if (str == "GT")                  return AST::ID::GT;
  else if (str == "IF")                  return AST::ID::IF;
  else if (str == "INDEX")               return AST::ID::INDEX;
  else if (str == "INT_LIT")             return AST::ID::INT_LIT;
  else if (str == "LE")                  return AST::ID::LE;
  else if (str == "LET")                 return AST::ID::LET;
  else if (str == "LT")                  return AST::ID::LT;
  else if (str == "MUL")                 return AST::ID::MUL;
  else if (str == "NE")                  return AST::ID::NE;
  else if (str == "REF_EXP")             return AST::ID::REF_EXP;
  else if (str == "RETURN")              return AST::ID::RETURN;
  else if (str == "STORE")               return AST::ID::STORE;
  else if (str == "STRING_LIT")          return AST::ID::STRING_LIT;
  else if (str == "SUB")                 return AST::ID::SUB;
  else if (str == "TRUE")                return AST::ID::TRUE;
  else if (str == "WREF_EXP")            return AST::ID::WREF_EXP;

  else if (str == "EXTERN_FUNC")         return AST::ID::EXTERN_FUNC;
  else if (str == "FUNC")                return AST::ID::FUNC;
  else if (str == "MODULE")              return AST::ID::MODULE;
  else if (str == "NAMESPACE")           return AST::ID::NAMESPACE;
  
  else if (str == "ARRAY_TEXP")          return AST::ID::ARRAY_TEXP;
  else if (str == "BOOL_TEXP")           return AST::ID::BOOL_TEXP;
  else if (str == "f32_TEXP")            return AST::ID::f32_TEXP;
  else if (str == "f64_TEXP")            return AST::ID::f64_TEXP;
  else if (str == "i8_TEXP")             return AST::ID::i8_TEXP;
  else if (str == "i16_TEXP")            return AST::ID::i16_TEXP;
  else if (str == "i32_TEXP")            return AST::ID::i32_TEXP;
  else if (str == "i64_TEXP")            return AST::ID::i64_TEXP;
  else if (str == "REF_TEXP")            return AST::ID::REF_TEXP;
  else if (str == "STR_TEXP")            return AST::ID::STR_TEXP;
  else if (str == "UNIT_TEXP")           return AST::ID::UNIT_TEXP;
  else if (str == "WREF_TEXP")           return AST::ID::WREF_TEXP;
  
  else if (str == "DECLLIST_CONS")       return AST::ID::DECLLIST_CONS;
  else if (str == "DECLLIST_NIL")        return AST::ID::DECLLIST_NIL;
  else if (str == "EXPLIST_CONS")        return AST::ID::EXPLIST_CONS;
  else if (str == "EXPLIST_NIL")         return AST::ID::EXPLIST_NIL;
  else if (str == "FQIDENT")             return AST::ID::FQIDENT;
  else if (str == "IDENT")               return AST::ID::IDENT;
  else if (str == "PARAMLIST_CONS")      return AST::ID::PARAMLIST_CONS;
  else if (str == "PARAMLIST_NIL")       return AST::ID::PARAMLIST_NIL;
  else if (str == "QIDENT")              return AST::ID::QIDENT;

  else { printf("Invalid AST::ID string: %s\n", str.c_str()); exit(1); }
}

/** Returns the sub-ASTs of `node`. This is only used for testing purposes. */
std::vector<Addr<AST>> getSubnodes(const ASTContext& ctx, Addr<AST> node) {
  const AST* nodePtr = ctx.getConstPtr(node);
  AST::ID id = nodePtr->getID();
  std::vector<Addr<AST>> ret;

  // TODO: make this a switch statement
  if (nodePtr->isBinopExp()) {
    auto n = reinterpret_cast<const BinopExp*>(nodePtr);
    ret.push_back(n->getLHS().upcast<AST>());
    ret.push_back(n->getRHS().upcast<AST>());
  } else if (id == AST::ID::ARRAY_INIT) {
    auto n = reinterpret_cast<const ArrayInitExp*>(nodePtr);
    ret.push_back(n->getSize().upcast<AST>());
    ret.push_back(n->getInitializer().upcast<AST>());
  } else if (id == AST::ID::ARRAY_LIST) {
    auto n = reinterpret_cast<const ArrayListExp*>(nodePtr);
    ret.push_back(n->getContent().upcast<AST>());
  } else if (id == AST::ID::ARRAY_TEXP) {
    auto n = reinterpret_cast<const ArrayTypeExp*>(nodePtr);
    if (n->getSize().exists())
      ret.push_back(n->getSize().upcast<AST>());
    ret.push_back(n->getInnerType().upcast<AST>());
  } else if (id == AST::ID::ASCRIP) {
    auto n = reinterpret_cast<const AscripExp*>(nodePtr);
    ret.push_back(n->getAscriptee().upcast<AST>());
    ret.push_back(n->getAscripter().upcast<AST>());
  } else if (id == AST::ID::BLOCK) {
    auto n = reinterpret_cast<const BlockExp*>(nodePtr);
    ret.push_back(n->getStatements().upcast<AST>());
  } else if (id == AST::ID::CALL) {
    auto n = reinterpret_cast<const CallExp*>(nodePtr);
    ret.push_back(n->getFunction().upcast<AST>());
    ret.push_back(n->getArguments().upcast<AST>());
  } else if (id == AST::ID::DECLLIST_CONS) {
    auto n = reinterpret_cast<const DeclList*>(nodePtr);
    ret.push_back(n->getHead().upcast<AST>());
    ret.push_back(n->getTail().upcast<AST>());
  } else if (id == AST::ID::DEREF) {
    auto n = reinterpret_cast<const DerefExp*>(nodePtr);
    ret.push_back(n->getOf().upcast<AST>());
  } else if (id == AST::ID::ENAME) {
    auto n = reinterpret_cast<const NameExp*>(nodePtr);
    ret.push_back(n->getName().upcast<AST>());
  } else if (id == AST::ID::EXPLIST_CONS) {
    auto n = reinterpret_cast<const ExpList*>(nodePtr);
    ret.push_back(n->getHead().upcast<AST>());
    ret.push_back(n->getTail().upcast<AST>());
  } else if (id == AST::ID::EXTERN_FUNC) {
    auto n = reinterpret_cast<const FunctionDecl*>(nodePtr);
    ret.push_back(n->getName().upcast<AST>());
    ret.push_back(n->getParameters().upcast<AST>());
    ret.push_back(n->getReturnType().upcast<AST>());
  } else if (id == AST::ID::FUNC) {
    auto n = reinterpret_cast<const FunctionDecl*>(nodePtr);
    ret.push_back(n->getName().upcast<AST>());
    ret.push_back(n->getParameters().upcast<AST>());
    ret.push_back(n->getReturnType().upcast<AST>());
    ret.push_back(n->getBody().upcast<AST>());
  } else if (id == AST::ID::IF) {
    auto n = reinterpret_cast<const IfExp*>(nodePtr);
    ret.push_back(n->getCondExp().upcast<AST>());
    ret.push_back(n->getThenExp().upcast<AST>());
    ret.push_back(n->getElseExp().upcast<AST>());
  } else if (id == AST::ID::INDEX) {
    auto n = reinterpret_cast<const IndexExp*>(nodePtr);
    ret.push_back(n->getBase().upcast<AST>());
    ret.push_back(n->getIndex().upcast<AST>());
  } else if (id == AST::ID::LET) {
    auto n = reinterpret_cast<const LetExp*>(nodePtr);
    ret.push_back(n->getBoundIdent().upcast<AST>());
    if (n->getAscrip().exists())
      ret.push_back(n->getAscrip().upcast<AST>());
    ret.push_back(n->getDefinition().upcast<AST>());
  } else if (id == AST::ID::MODULE) {
    auto n = reinterpret_cast<const ModuleDecl*>(nodePtr);
    ret.push_back(n->getName().upcast<AST>());
    ret.push_back(n->getDecls().upcast<AST>());
  } else if (id == AST::ID::NAMESPACE) {
    auto n = reinterpret_cast<const NamespaceDecl*>(nodePtr);
    ret.push_back(n->getName().upcast<AST>());
    ret.push_back(n->getDecls().upcast<AST>());
  } else if (id == AST::ID::PARAMLIST_CONS) {
    auto n = reinterpret_cast<const ParamList*>(nodePtr);
    ret.push_back(n->getHeadParamName().upcast<AST>());
    ret.push_back(n->getHeadParamType().upcast<AST>());
    ret.push_back(n->getTail().upcast<AST>());
  } else if (id == AST::ID::QIDENT) {
    auto n = reinterpret_cast<const QIdent*>(nodePtr);
    ret.push_back(n->getHead().upcast<AST>());
    ret.push_back(n->getTail().upcast<AST>());
  } else if (id == AST::ID::REF_EXP || id == AST::ID::WREF_EXP) {
    auto n = reinterpret_cast<const RefExp*>(nodePtr);
    ret.push_back(n->getInitializer().upcast<AST>());
  } else if (id == AST::ID::REF_TEXP || id == AST::ID::WREF_TEXP) {
    auto n = reinterpret_cast<const RefTypeExp*>(nodePtr);
    ret.push_back(n->getPointeeType().upcast<AST>());
  } else if (id == AST::ID::RETURN) {
    auto n = reinterpret_cast<const ReturnExp*>(nodePtr);
    ret.push_back(n->getReturnee().upcast<AST>());
  } else if (id == AST::ID::STORE) {
    auto n = reinterpret_cast<const StoreExp*>(nodePtr);
    ret.push_back(n->getLHS().upcast<AST>());
    ret.push_back(n->getRHS().upcast<AST>());
  }
  return ret;
}

#endif