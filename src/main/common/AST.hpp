#ifndef COMMON_AST
#define COMMON_AST

#include <cstdio>
#include <string>
#include <llvm/ADT/StringRef.h>
#include "common/Location.hpp"

class Exp;
class IntLit;

/// @brief An AST node is any syntactic form that appears in source code. All
/// AST nodes have an `id`, which is used to downcast to subclasses, and a
/// source code `location`.
class AST {
public:
  enum ID : unsigned short {
    // expressions and statements
    ADD, ARRAY_INIT, ARRAY_LIST, ASCRIP, BLOCK, CALL, DEC_LIT, DEREF, DIV,
    ENAME, EQ, FALSE, GE, GT, IF, INDEX, INT_LIT, LE, LET, LT, MUL, NE, REF_EXP,
    RETURN, STORE, STRING_LIT, SUB, TRUE, WREF_EXP,

    // declarations
    DATA, EXTERN_FUNC, FUNC, MODULE, NAMESPACE,

    // type expressions
    ARRAY_TEXP, BOOL_TEXP, f32_TEXP, f64_TEXP, i8_TEXP, i16_TEXP, i32_TEXP,
    i64_TEXP, REF_TEXP, STR_TEXP, UNIT_TEXP, WREF_TEXP, 

    // other
    DECLLIST_CONS, DECLLIST_NIL, EXPLIST_CONS, EXPLIST_NIL, NAME,
    PARAMLIST_CONS, PARAMLIST_NIL,
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
  union { unsigned int compileTime; Exp* runtime; } arraySize;
  TVar inner;
  Type(Type::ID id) : arraySize({.compileTime=0}) { this->id = id; }
  Type(Type::ID id, TVar inner) : arraySize({.compileTime=0}) {
    this->id = id; this->inner = inner;
  }
  Type(Exp* arraySize, TVar of) : arraySize({.runtime = arraySize}) {
    this->id = ID::ARRAY_SART; this->inner = of;
  }
  Type(unsigned int arrSize, TVar of) : arraySize({.compileTime = arrSize}) {
    this->id = ID::ARRAY_SACT; this->inner = of;
  }
public:
  /// @brief Construct an array type sized at runtime. 
  static Type array_sart(Exp* sz, TVar of) { return Type(sz, of); }
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
  Exp* getRuntimeArraySize() const { return arraySize.runtime; }
};

class Name : public AST {
  std::string s;
public:
  Name(Location loc, std::string s) : AST(NAME, loc), s(s) {}
  static Name* downcast(AST* ast) {
    return ast->getID() == NAME ? static_cast<Name*>(ast) : nullptr;
  }
  llvm::StringRef asStringRef() const { return s; }
  /// @brief Sets this name to `s`.
  void set(llvm::StringRef s) { this->s.assign(s.data(), s.size()); }
};

////////////////////////////////////////////////////////////////////////////////
// TYPE EXPRESSIONS
////////////////////////////////////////////////////////////////////////////////

/// @brief A type that appears in the source text.
class TypeExp : public AST {
protected:
  TypeExp(ID id, Location loc) : AST(id, loc) {}
public:
  static TypeExp* newUnit(Location loc) { return new TypeExp(UNIT_TEXP, loc); }
  static TypeExp* newBool(Location loc) { return new TypeExp(BOOL_TEXP, loc); }
  static TypeExp* newI8(Location loc) { return new TypeExp(i8_TEXP, loc); }
  static TypeExp* newI16(Location loc) { return new TypeExp(i16_TEXP, loc); }
  static TypeExp* newI32(Location loc) { return new TypeExp(i32_TEXP, loc); }
  static TypeExp* newI64(Location loc) { return new TypeExp(i64_TEXP, loc); }
  static TypeExp* newF32(Location loc) { return new TypeExp(f32_TEXP, loc); }
  static TypeExp* newF64(Location loc) { return new TypeExp(f64_TEXP, loc); }
  static TypeExp* newStr(Location loc) { return new TypeExp(STR_TEXP, loc); }
};

/// @brief A read-only or writable reference type expression.
class RefTypeExp : public TypeExp {
  TypeExp* pointeeType;
  bool writable;
public:
  RefTypeExp(Location loc, TypeExp* pointeeType, bool writable)
      : TypeExp(writable ? WREF_TEXP : REF_TEXP, loc) {
    this->pointeeType = pointeeType;
    this->writable = writable;
  }
  static RefTypeExp* downcast(AST* ast) {
    return (ast->getID() == REF_TEXP || ast->getID() == WREF_TEXP) ?
           static_cast<RefTypeExp*>(ast) : nullptr;
  }
  TypeExp* getPointeeType() const { return pointeeType; }
  bool isWritable() const { return writable; }
};

/// @brief Type expression of a constant-size array `[20 of i32]` or
/// unknown-size array `[n of i32]` or `[_ of i32]`.
class ArrayTypeExp : public TypeExp {
  Exp* size;
  TypeExp* innerType;
public:
  /// @brief If `size` is an error address, then an underscore was used to
  /// indicate the array size. 
  ArrayTypeExp(Location loc, Exp* size, TypeExp* innerType)
      : TypeExp(ARRAY_TEXP, loc) {
    this->size = size; this->innerType = innerType;
  }
  static ArrayTypeExp* downcast(AST* ast) {
    return ast->getID() == ARRAY_TEXP ?
           static_cast<ArrayTypeExp*>(ast) : nullptr;
  }
  /// @brief This returns nullptr if size is unknown.
  Exp* getSize() const { return size; }
  TypeExp* getInnerType() const { return innerType; }
  bool hasUnderscoreSize() const { return size == nullptr; }
};

////////////////////////////////////////////////////////////////////////////////
// EXPRESSIONS
////////////////////////////////////////////////////////////////////////////////

/// @brief An expression or statement (currently there is no distinction).
class Exp : public AST {
protected:
  TVar type = TVar::none();
  Exp(ID id, Location loc) : AST(id, loc) {}
public:
  static Exp* downcast(AST* ast) {
    return ast->isExp() ? static_cast<Exp*>(ast) : nullptr;
  }
  TVar getTVar() const { return type; };
  void setTVar(TVar type) { this->type = type; }
};

/// @brief A linked-list of expressions.
class ExpList : public AST {
  Exp* head;
  ExpList* tail;
public:
  ExpList(Location loc) : AST(EXPLIST_NIL, loc) {
    this->head = nullptr;
    this->tail = nullptr;
  }
  ExpList(Location loc, Exp* head, ExpList* tail)
      : AST(EXPLIST_CONS, loc) {
    this->head = head;
    this->tail = tail;
  }
  bool isEmpty() const { return getID() == EXPLIST_NIL; }
  bool nonEmpty() const { return getID() == EXPLIST_CONS; }

  /// Returns the first expression in this list. This is only safe to call
  /// if `isEmpty` returns false.
  Exp* getHead() const { return head; }

  /// Returns the tail of this expression list. This is only safe to call
  /// if `isEmpty` returns false;
  ExpList* getTail() const { return tail; }
};

/// @brief A boolean literal (`true` or `false`).
class BoolLit : public Exp {
  bool value;
public:
  BoolLit(Location loc, bool value) : Exp(value ? TRUE : FALSE, loc) {
    this->value = value;
  }
  static BoolLit* downcast(AST* ast) {
    return ast->getID() == TRUE || ast->getID() == FALSE ?
           static_cast<BoolLit*>(ast) : nullptr;
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
  static IntLit* downcast(AST* ast) {
    return ast->getID() == INT_LIT ? static_cast<IntLit*>(ast) : nullptr;
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
  static DecimalLit* downcast(AST* ast) {
    return ast->getID() == DEC_LIT ? static_cast<DecimalLit*>(ast) : nullptr;
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
  static StringLit* downcast(AST* ast) {
    return ast->getID() == STRING_LIT ? static_cast<StringLit*>(ast) : nullptr;
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
  Name* name;
public:
  NameExp(Location loc, Name* name) : Exp(ENAME, loc) {
    this->name = name;
  }
  static NameExp* downcast(AST* ast) {
    return ast->getID() == ENAME ? static_cast<NameExp*>(ast) : nullptr;
  }
  Name* getName() const { return name; }
};

/// @brief A binary operator expression.
class BinopExp : public Exp {
  Exp* lhs;
  Exp* rhs;
public:
  BinopExp(ID id, Location loc, Exp* lhs, Exp* rhs) : Exp(id, loc) {
    this->lhs = lhs;
    this->rhs = rhs;
  }
  static BinopExp* downcast(AST* ast) {
    return ast->isBinopExp() ? static_cast<BinopExp*>(ast) : nullptr;
  }
  Exp* getLHS() const { return lhs; }
  Exp* getRHS() const { return rhs; }
};

/// @brief An if-then-else expression.
class IfExp : public Exp {
  Exp* condExp;
  Exp* thenExp;
  Exp* elseExp;
public:
  IfExp(Location loc, Exp* condExp, Exp* thenExp, Exp* elseExp)
      : Exp(IF, loc) {
    this->condExp = condExp;
    this->thenExp = thenExp;
    this->elseExp = elseExp;
  }
  static IfExp* downcast(AST* ast) {
    return ast->getID() == IF ? static_cast<IfExp*>(ast) : nullptr;
  }
  Exp* getCondExp() const { return condExp; }
  Exp* getThenExp() const { return thenExp; }
  Exp* getElseExp() const { return elseExp; }
};

/// @brief A block expression.
class BlockExp : public Exp {
  ExpList* statements;
public:
  BlockExp(Location loc, ExpList* statements) : Exp(BLOCK, loc) {
    this->statements = statements;
  }
  static BlockExp* downcast(AST* ast) {
    return ast->getID() == BLOCK ? static_cast<BlockExp*>(ast) : nullptr;
  }
  ExpList* getStatements() const { return statements; }
};

/// @brief A function call expression.
class CallExp : public Exp {
  Name* function;
  ExpList* arguments;
public:
  CallExp(Location loc, Name* function, ExpList* arguments)
      : Exp(CALL, loc) {
    this->function = function;
    this->arguments = arguments;
  }
  static CallExp* downcast(AST* ast) {
    return ast->getID() == CALL ? static_cast<CallExp*>(ast) : nullptr;
  }
  Name* getFunction() const { return function; }
  ExpList* getArguments() const { return arguments; }
  /// TODO: should this copy the name or can it just store the pointer?
  void setFunction(Name* function) { this->function = function; }
};

/// @brief A type ascription expression.
class AscripExp : public Exp {
  Exp* ascriptee;
  TypeExp* ascripter;
public:
  AscripExp(Location loc, Exp* ascriptee, TypeExp* ascripter)
      : Exp(ASCRIP, loc) {
    this->ascriptee = ascriptee;
    this->ascripter = ascripter;
  }
  static AscripExp* downcast(AST* ast) {
    return ast->getID() == ASCRIP ? static_cast<AscripExp*>(ast) : nullptr;
  }
  Exp* getAscriptee() const { return ascriptee; }
  TypeExp* getAscripter() const { return ascripter; }
};

/// @brief A let statement. The type ascription is optional.
class LetExp : public Exp {
  Name* boundIdent;
  TypeExp* ascrip;
  Exp* definition;
public:
  LetExp(Location loc, Name* boundIdent, TypeExp* ascrip,
      Exp* definition) : Exp(LET, loc) {
    this->boundIdent = boundIdent;
    this->ascrip = ascrip;
    this->definition = definition;
  }
  static LetExp* downcast(AST* ast) {
    return ast->getID() == LET ? static_cast<LetExp*>(ast) : nullptr;
  }
  Name* getBoundIdent() const { return boundIdent; }
  TypeExp* getAscrip() const { return ascrip; }
  Exp* getDefinition() const { return definition; }
};

/// @brief A return statement.
class ReturnExp : public Exp {
  Exp* returnee;
public:
  ReturnExp(Location loc, Exp* returnee) : Exp(RETURN, loc) {
    this->returnee = returnee;
  }
  static ReturnExp* downcast(AST* ast) {
    return ast->getID() == RETURN ? static_cast<ReturnExp*>(ast) : nullptr;
  }
  Exp* getReturnee() const { return returnee; }
};

/// @brief A store statement (e.g., `x := 42`).
class StoreExp : public Exp {
  Exp* lhs;
  Exp* rhs;
public:
  StoreExp(Location loc, Exp* lhs, Exp* rhs) : Exp(STORE, loc) {
    this->lhs = lhs;
    this->rhs = rhs;
  }
  static StoreExp* downcast(AST* ast) {
    return ast->getID() == STORE ? static_cast<StoreExp*>(ast) : nullptr;
  }
  Exp* getLHS() const { return lhs; }
  Exp* getRHS() const { return rhs; }
};

/// @brief A read-only or writable reference constructor expression.
class RefExp : public Exp {
  Exp* initializer;
public:
  RefExp(Location loc, Exp* initializer, bool writable)
      : Exp(writable ? WREF_EXP : REF_EXP, loc) {
    this->initializer = initializer;
  }
  static RefExp* downcast(AST* ast) {
    return (ast->getID() == REF_EXP || ast->getID() == WREF_EXP) ?
           static_cast<RefExp*>(ast) : nullptr;
  }
  Exp* getInitializer() const { return initializer; }
};

/// @brief A dereference expression.
class DerefExp : public Exp {
  Exp* of;
public:
  DerefExp(Location loc, Exp* of) : Exp(DEREF, loc) { this->of = of; }
  static DerefExp* downcast(AST* ast) {
    return ast->getID() == DEREF ? static_cast<DerefExp*>(ast) : nullptr;
  }
  Exp* getOf() const { return of; }
};

/// @brief An array constructor that lists out the array contents.
/// e.g., `[42, x, y+1]`
class ArrayListExp : public Exp {
  ExpList* content;
public:
  ArrayListExp(Location loc, ExpList* content) : Exp(ARRAY_LIST, loc) {
    this->content = content;
  }
  static ArrayListExp* downcast(AST* ast) {
    return ast->getID() == ARRAY_LIST ?
           static_cast<ArrayListExp*>(ast) : nullptr;
  }
  ExpList* getContent() const { return content; }
};

/// @brief An expression that constructs an array by specifying a size and
/// an element to fill all the spots. e.g., `[10 of 0]`
class ArrayInitExp : public Exp {
  Exp* size;
  Exp* initializer;
public:
  ArrayInitExp(Location loc, Exp* size, Exp* initializer)
      : Exp(ARRAY_INIT, loc) {
    this->size = size;
    this->initializer = initializer;
  }
  static ArrayInitExp* downcast(AST* ast) {
    return ast->getID() == ARRAY_INIT ?
           static_cast<ArrayInitExp*>(ast) : nullptr;
  }
  Exp* getSize() const { return size; }
  Exp* getInitializer() const { return initializer; }
};

/// @brief An expression that calculates an address from a base reference and
/// offset (e.g., `myarrayref[3]`).
class IndexExp : public Exp {
  Exp* base;
  Exp* index;
public:
  IndexExp(Location loc, Exp* base, Exp* index) : Exp(INDEX, loc) {
    this->base = base; this->index = index;
  }
  static IndexExp* downcast(AST* ast) {
    return ast->getID() == INDEX ? static_cast<IndexExp*>(ast) : nullptr;
  }
  Exp* getBase() const { return base; }
  Exp* getIndex() const { return index; }
};

////////////////////////////////////////////////////////////////////////////////
// DECLARATIONS
////////////////////////////////////////////////////////////////////////////////

/// @brief A declaration.
class Decl : public AST {
protected:
  Name* name;
  Decl(ID id, Location loc, Name* name) : AST(id, loc) {
    this->name = name;
  }
public:
  Name* getName() const { return name; }
  /// @brief Safely sets the name of this decl to `name`. 
  void setName(llvm::StringRef name) { this->name->set(name); }
};

/// @brief A list of declarations.
class DeclList : public AST {
  Decl* head;
  DeclList* tail;
public:
  DeclList(Location loc) : AST(DECLLIST_NIL, loc) {
    this->head = nullptr;
    this->tail = nullptr;
  }
  DeclList(Location loc, Decl* head, DeclList* tail)
      : AST(DECLLIST_CONS, loc) {
    this->head = head;
    this->tail = tail;
  }
  bool isEmpty() { return getID() == DECLLIST_NIL; }
  bool nonEmpty() { return getID() == DECLLIST_CONS; }
  Decl* getHead() const { return head; }
  DeclList* getTail() const { return tail; }
};

class ModuleDecl : public Decl {
  DeclList* decls;
public:
  ModuleDecl(Location loc, Name* name, DeclList* decls)
      : Decl(MODULE, loc, name) {
    this->decls = decls;
  }
  static ModuleDecl* downcast(AST* ast) {
    return ast->getID() == MODULE ? static_cast<ModuleDecl*>(ast) : nullptr;
  }
  DeclList* getDecls() const { return decls; }
};

class NamespaceDecl : public Decl {
  DeclList* decls;
public:
  NamespaceDecl(Location loc, Name* name, DeclList* decls)
      : Decl(NAMESPACE, loc, name) {
    this->decls = decls;
  }
  static NamespaceDecl* downcast(AST* ast) {
    return ast->getID() == NAMESPACE ? static_cast<NamespaceDecl*>(ast) : nullptr;
  }
  DeclList* getDecls() const { return decls; }
};

/// @brief A list of parameters in a function signature, or a list of
/// fields in a data type.
class ParamList : public AST {
  Name* headParamName;
  TypeExp* headParamType;
  ParamList* tail;
public:
  ParamList(Location loc) : AST(PARAMLIST_NIL, loc) {
    this->headParamName = nullptr;
    this->headParamType = nullptr;
    this->tail = nullptr;
  }
  ParamList(Location loc, Name* headParamName,
      TypeExp* headParamType, ParamList* tail)
      : AST(PARAMLIST_CONS, loc) {
    this->headParamName = headParamName;
    this->headParamType = headParamType;
    this->tail = tail;
  }
  bool isEmpty() { return getID() == PARAMLIST_NIL; }
  bool nonEmpty() { return getID() == PARAMLIST_CONS; }
  Name* getHeadParamName() const { return headParamName; }
  TypeExp* getHeadParamType() const { return headParamType; }
  ParamList* getTail() const { return tail; }
};

/// @brief A function or extern function (FUNC/EXTERN_FUNC).
class FunctionDecl : public Decl {
  ParamList* parameters;
  TypeExp* returnType;
  Exp* body;
public:
  FunctionDecl(Location loc, Name* name, ParamList* parameters,
      TypeExp* returnType, Exp* body) : Decl(FUNC, loc, name) {
    this->parameters = parameters;
    this->returnType = returnType;
    this->body = body;
  }
  FunctionDecl(Location loc, Name* name, ParamList* parameters,
      TypeExp* returnType) : Decl(EXTERN_FUNC, loc, name) {
    this->parameters = parameters;
    this->returnType = returnType;
    this->body = nullptr;
  }
  static FunctionDecl* downcast(AST* ast) {
    return ast->getID() == FUNC || ast->getID() == EXTERN_FUNC ?
           static_cast<FunctionDecl*>(ast) : nullptr;
  }
  ParamList* getParameters() const { return parameters; }
  TypeExp* getReturnType() const { return returnType; }
  Exp* getBody() const { return body; }
};

/// @brief The declaration of a data type.
class DataDecl : public Decl {
  ParamList* fields;
public:
  DataDecl(Location loc, Name* name, ParamList* fields)
      : Decl(DATA, loc, name) { this->fields = fields; }
  static DataDecl* downcast(AST* ast) {
    return ast->getID() == DATA ? static_cast<DataDecl*>(ast) : nullptr;
  }
  ParamList* getFields() const { return fields; }
};

////////////////////////////////////////////////////////////////////////////////

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

  case AST::ID::DATA:               return "DATA";
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
  case AST::ID::NAME:               return "NAME";
  case AST::ID::PARAMLIST_CONS:     return "PARAMLIST_CONS";
  case AST::ID::PARAMLIST_NIL:      return "PARAMLIST_NIL";
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

  else if (str == "DATA")                return AST::ID::DATA;
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
  else if (str == "NAME")                return AST::ID::NAME;
  else if (str == "PARAMLIST_CONS")      return AST::ID::PARAMLIST_CONS;
  else if (str == "PARAMLIST_NIL")       return AST::ID::PARAMLIST_NIL;

  else { printf("Invalid AST::ID string: %s\n", str.c_str()); exit(1); }
}

/** Returns the sub-ASTs of `node`. This is only used for testing purposes. */
std::vector<AST*> getSubnodes(AST* ast) {
  AST::ID id = ast->getID();
  std::vector<AST*> ret;

  // TODO: make this a switch statement
  if (ast->isBinopExp()) {
    auto n = reinterpret_cast<const BinopExp*>(ast);
    ret.push_back(n->getLHS());
    ret.push_back(n->getRHS());
  } else if (id == AST::ID::ARRAY_INIT) {
    auto n = reinterpret_cast<const ArrayInitExp*>(ast);
    ret.push_back(n->getSize());
    ret.push_back(n->getInitializer());
  } else if (id == AST::ID::ARRAY_LIST) {
    auto n = reinterpret_cast<const ArrayListExp*>(ast);
    ret.push_back(n->getContent());
  } else if (id == AST::ID::ARRAY_TEXP) {
    auto n = reinterpret_cast<const ArrayTypeExp*>(ast);
    if (n->getSize() != nullptr)
      ret.push_back(n->getSize());
    ret.push_back(n->getInnerType());
  } else if (id == AST::ID::ASCRIP) {
    auto n = reinterpret_cast<const AscripExp*>(ast);
    ret.push_back(n->getAscriptee());
    ret.push_back(n->getAscripter());
  } else if (id == AST::ID::BLOCK) {
    auto n = reinterpret_cast<const BlockExp*>(ast);
    ret.push_back(n->getStatements());
  } else if (id == AST::ID::CALL) {
    auto n = reinterpret_cast<const CallExp*>(ast);
    ret.push_back(n->getFunction());
    ret.push_back(n->getArguments());
  } else if (id == AST::ID::DATA) {
    auto n = reinterpret_cast<const DataDecl*>(ast);
    ret.push_back(n->getName());
    ret.push_back(n->getFields());
  } else if (id == AST::ID::DECLLIST_CONS) {
    auto n = reinterpret_cast<const DeclList*>(ast);
    ret.push_back(n->getHead());
    ret.push_back(n->getTail());
  } else if (id == AST::ID::DEREF) {
    auto n = reinterpret_cast<const DerefExp*>(ast);
    ret.push_back(n->getOf());
  } else if (id == AST::ID::ENAME) {
    auto n = reinterpret_cast<const NameExp*>(ast);
    ret.push_back(n->getName());
  } else if (id == AST::ID::EXPLIST_CONS) {
    auto n = reinterpret_cast<const ExpList*>(ast);
    ret.push_back(n->getHead());
    ret.push_back(n->getTail());
  } else if (id == AST::ID::EXTERN_FUNC) {
    auto n = reinterpret_cast<const FunctionDecl*>(ast);
    ret.push_back(n->getName());
    ret.push_back(n->getParameters());
    ret.push_back(n->getReturnType());
  } else if (id == AST::ID::FUNC) {
    auto n = reinterpret_cast<const FunctionDecl*>(ast);
    ret.push_back(n->getName());
    ret.push_back(n->getParameters());
    ret.push_back(n->getReturnType());
    ret.push_back(n->getBody());
  } else if (id == AST::ID::IF) {
    auto n = reinterpret_cast<const IfExp*>(ast);
    ret.push_back(n->getCondExp());
    ret.push_back(n->getThenExp());
    ret.push_back(n->getElseExp());
  } else if (id == AST::ID::INDEX) {
    auto n = reinterpret_cast<const IndexExp*>(ast);
    ret.push_back(n->getBase());
    ret.push_back(n->getIndex());
  } else if (id == AST::ID::LET) {
    auto n = reinterpret_cast<const LetExp*>(ast);
    ret.push_back(n->getBoundIdent());
    if (n->getAscrip() != nullptr)
      ret.push_back(n->getAscrip());
    ret.push_back(n->getDefinition());
  } else if (id == AST::ID::MODULE) {
    auto n = reinterpret_cast<const ModuleDecl*>(ast);
    ret.push_back(n->getName());
    ret.push_back(n->getDecls());
  } else if (id == AST::ID::NAMESPACE) {
    auto n = reinterpret_cast<const NamespaceDecl*>(ast);
    ret.push_back(n->getName());
    ret.push_back(n->getDecls());
  } else if (id == AST::ID::PARAMLIST_CONS) {
    auto n = reinterpret_cast<const ParamList*>(ast);
    ret.push_back(n->getHeadParamName());
    ret.push_back(n->getHeadParamType());
    ret.push_back(n->getTail());
  } else if (id == AST::ID::REF_EXP || id == AST::ID::WREF_EXP) {
    auto n = reinterpret_cast<const RefExp*>(ast);
    ret.push_back(n->getInitializer());
  } else if (id == AST::ID::REF_TEXP || id == AST::ID::WREF_TEXP) {
    auto n = reinterpret_cast<const RefTypeExp*>(ast);
    ret.push_back(n->getPointeeType());
  } else if (id == AST::ID::RETURN) {
    auto n = reinterpret_cast<const ReturnExp*>(ast);
    ret.push_back(n->getReturnee());
  } else if (id == AST::ID::STORE) {
    auto n = reinterpret_cast<const StoreExp*>(ast);
    ret.push_back(n->getLHS());
    ret.push_back(n->getRHS());
  }
  return ret;
}

#endif