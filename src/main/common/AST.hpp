#ifndef COMMON_AST
#define COMMON_AST

#include <cstdio>
#include <string>
#include "common/ASTContext.hpp"
#include "common/Location.hpp"

class IntLit;

/// TODO: can we include the pointer to the ontology in this struct? 
class FQNameKey {
  unsigned int value;
public:
  FQNameKey() { value = -1; }
  FQNameKey(unsigned int value) { this->value = value; }
  unsigned int getValue() const { return value; }
};

/// @brief Base class for all AST elements.
class AST {
public:
  enum ID : unsigned short {
    // expressions and statements
    ADD, ARRAY_INIT, ARRAY_LIST, ASCRIP, BLOCK, CALL, DEC_LIT, DIV, ENAME, EQ,
    FALSE, IF, INT_LIT, LET, MUL, NE, REF_EXP, RETURN, STORE, STRING_LIT, SUB,
    TRUE, WREF_EXP,

    // declarations
    EXTERN_FUNC, FUNC, MODULE, NAMESPACE,

    // type expressions
    ARRAY_TEXP, BOOL_TEXP, f32_TEXP, f64_TEXP, i8_TEXP, i32_TEXP, REF_TEXP,
    UNIT_TEXP, WREF_TEXP, 

    // other
    DECLLIST_CONS, DECLLIST_NIL, EXPLIST_CONS, EXPLIST_NIL, FQIDENT, IDENT,
    PARAMLIST_CONS, PARAMLIST_NIL, QIDENT,
  };

private:
  ID id;

protected:
  AST(ID id) { this->id = id; }

public:
  ID getID() const { return id; }

  bool isExp() {
    switch (id) {
    case ADD: case ARRAY_INIT: case ARRAY_LIST: case ASCRIP: case BLOCK:
    case CALL: case DEC_LIT: case DIV: case ENAME: case EQ: case FALSE: case IF:
    case INT_LIT: case LET: case MUL: case NE: case REF_EXP: case RETURN:
    case STORE: case STRING_LIT: case SUB: case TRUE: case WREF_EXP:
      return true;
    default: return false;
    }
  }

};

/// @brief A type variable that annotates an expression.
class TVar {
  int value;
public:
  static TVar none() { return TVar(-1); }
  TVar() { this->value = -1; }
  TVar(int value) { this->value = value; }
  int get() const { return value; }
  bool exists() { return value >= 0; }
};

/// @brief A type.
class Type {
public:
  enum struct ID : unsigned char {
    ARRAY, BOOL, DECIMAL, f32, f64, i8, i32, NOTYPE, NUMERIC, REF, UNIT, WREF
  };
protected:
  Type::ID id;
  TVar inner = TVar::none();
  Type(Type::ID id) { this->id = id; this->inner = TVar::none(); }
  Type(Type::ID id, TVar inner) { this->id = id; this->inner = inner; }
public:
  static Type array(TVar of) { return Type(ID::ARRAY, of); }
  static Type bool_() { return Type(ID::BOOL); }
  static Type decimal() { return Type(ID::DECIMAL); }
  static Type f32() { return Type(ID::f32); }
  static Type f64() { return Type(ID::f64); }
  static Type i8() { return Type(ID::i8); }
  static Type i32() { return Type(ID::i32); }
  static Type notype() { return Type(ID::NOTYPE); }
  static Type numeric() { return Type(ID::NUMERIC); }
  static Type ref(TVar of) { return Type(ID::REF, of); }
  static Type unit() { return Type(ID::UNIT); }
  static Type wref(TVar of) { return Type(ID::WREF, of); }
  bool isNoType() { return id == ID::NOTYPE; }
  ID getID() { return id; }
  TVar getInner() { return inner; }
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

/// @brief An AST element that has a location in the source text.
class LocatedAST : public AST {
  Location location;
protected:
  LocatedAST(ID id, Location location) : AST(id) {
    this->location = location;
  }
public:
  static LocatedAST* fromAST(AST* ast) {
    return static_cast<LocatedAST*>(ast);
  }
  Location getLocation() const { return location; }
};

/// @brief A qualified, unqualified, or fully qualified identifier
/// (QIDENT, IDENT, FQIDENT).
class Name : public LocatedAST {
protected:
  Name(ID id, Location loc) : LocatedAST(id, loc) {}
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
class TypeExp : public LocatedAST {
protected:
  TypeExp(ID id, Location loc) : LocatedAST(id, loc) {}
public:
  static TypeExp mkUnit(Location loc) { return TypeExp(UNIT_TEXP, loc); }
  static TypeExp mkBool(Location loc) { return TypeExp(BOOL_TEXP, loc); }
  static TypeExp mkI8(Location loc) { return TypeExp(i8_TEXP, loc); }
  static TypeExp mkI32(Location loc) { return TypeExp(i32_TEXP, loc); }
  static TypeExp mkF32(Location loc) { return TypeExp(f32_TEXP, loc); }
  static TypeExp mkF64(Location loc) { return TypeExp(f64_TEXP, loc); }
};

class RefTypeExp : public TypeExp {
  Addr<TypeExp> pointeeType;
public:
  RefTypeExp(Location loc, Addr<TypeExp> pointeeType) : TypeExp(REF_TEXP, loc) {
    this->pointeeType = pointeeType;
  }
  Addr<TypeExp> getPointeeType() const { return pointeeType; }
};

class WRefTypeExp : public TypeExp {
  Addr<TypeExp> pointeeType;
public:
  WRefTypeExp(Location loc, Addr<TypeExp> pointeeType)
      : TypeExp(WREF_TEXP, loc) {
    this->pointeeType = pointeeType;
  }
  Addr<TypeExp> getPointeeType() const { return pointeeType; }
};

class ArrayTypeExp : public TypeExp {
  Addr<IntLit> sizeLit;
  Addr<TypeExp> innerType;
public:
  ArrayTypeExp(Location loc, Addr<IntLit> sizeLit, Addr<TypeExp> innerType)
      : TypeExp(ARRAY_TEXP, loc) {
    this->sizeLit = sizeLit;
    this->innerType = innerType;
  }
  Addr<IntLit> getSizeLit() const { return sizeLit; }
  Addr<TypeExp> getInnerType() const { return innerType; }
};

/// @brief An expression or statement (currently there is no distinction).
class Exp : public LocatedAST {
protected:
  TVar type = TVar::none();
  Exp(ID id, Location loc) : LocatedAST(id, loc) {}
public:
  TVar getTVar() const { return type; };
  void setTVar(TVar type) { this->type = type; }
};

/// @brief A linked-list of expressions.
class ExpList : public LocatedAST {
  Addr<Exp> head;
  Addr<ExpList> tail;
public:
  ExpList(Location loc) : LocatedAST(EXPLIST_NIL, loc) {
    this->head = Addr<Exp>::none();
    this->tail = Addr<ExpList>::none();
  }
  ExpList(Location loc, Addr<Exp> head, Addr<ExpList> tail)
      : LocatedAST(EXPLIST_CONS, loc) {
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
class ParamList : public LocatedAST {
  Addr<Ident> headParamName;
  Addr<TypeExp> headParamType;
  Addr<ParamList> tail;
public:
  ParamList(Location loc) : LocatedAST(PARAMLIST_NIL, loc) {
    this->headParamName = Addr<Ident>::none();
    this->headParamType = Addr<TypeExp>::none();
    this->tail = Addr<ParamList>::none();
  }
  ParamList(Location loc, Addr<Ident> headParamName,
      Addr<TypeExp> headParamType, Addr<ParamList> tail)
      : LocatedAST(PARAMLIST_CONS, loc) {
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
  std::string asString() { return std::string(ptr, getLocation().sz); }
};

/// @brief A decimal literal.
class DecimalLit : public Exp {
  const char* ptr;
public:
  DecimalLit(Location loc, const char* ptr) : Exp(DEC_LIT, loc) {
    this->ptr = ptr;
  }
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

/// @brief A let statement.
class LetExp : public Exp {
  Addr<Ident> boundIdent;
  Addr<Exp> definition;
public:
  LetExp(Location loc, Addr<Ident> boundIdent, Addr<Exp> definition)
      : Exp(LET, loc) {
    this->boundIdent = boundIdent;
    this->definition = definition;
  }
  Addr<Ident> getBoundIdent() const { return boundIdent; }
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

/// @brief A read-only reference constructor expression.
class RefExp : public Exp {
  Addr<Exp> initializer;
public:
  RefExp(Location loc, Addr<Exp> initializer) : Exp(REF_EXP, loc) {
    this->initializer = initializer;
  }
  Addr<Exp> getInitializer() const { return initializer; }
};

/// @brief A writable reference constructor expression.
class WRefExp : public Exp {
  Addr<Exp> initializer;
public:
  WRefExp(Location loc, Addr<Exp> initializer) : Exp(WREF_EXP, loc) {
    this->initializer = initializer;
  }
  Addr<Exp> getInitializer() const { return initializer; }
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
  Addr<IntLit> sizeLit;
  Addr<Exp> initializer;
public:
  ArrayInitExp(Location loc, Addr<IntLit> sizeLit, Addr<Exp> initializer)
      : Exp(ARRAY_INIT, loc) {
    this->sizeLit = sizeLit;
    this->initializer = initializer;
  }
  Addr<IntLit> getSizeLit() const { return sizeLit; }
  Addr<Exp> getInitializer() const { return initializer; }
};

/// @brief A declaration.
class Decl : public LocatedAST {
protected:
  Addr<Name> name;
  Decl(ID id, Location loc, Addr<Name> name) : LocatedAST(id, loc) {
    this->name = name;
  }
public:
  Addr<Name> getName() const { return name; }
  void setName(Addr<FQIdent> fqName) { name = fqName.upcast<Name>(); }
};

/// @brief A list of declarations.
class DeclList : public LocatedAST {
  Addr<Decl> head;
  Addr<DeclList> tail;
public:
  DeclList(Location loc) : LocatedAST(DECLLIST_NIL, loc) {
    this->head = Addr<Decl>::none();
    this->tail = Addr<DeclList>::none();
  }
  DeclList(Location loc, Addr<Decl> head, Addr<DeclList> tail)
      : LocatedAST(DECLLIST_CONS, loc) {
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
  case AST::ID::DIV:                return "DIV";
  case AST::ID::ENAME:              return "ENAME";
  case AST::ID::EQ:                 return "EQ";
  case AST::ID::FALSE:              return "FALSE";
  case AST::ID::IF:                 return "IF";
  case AST::ID::INT_LIT:            return "INT_LIT";
  case AST::ID::LET:                return "LET";
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
  case AST::ID::i32_TEXP:           return "i32_TEXP";
  case AST::ID::REF_TEXP:           return "REF_TEXP";
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
  else if (str == "DIV")                 return AST::ID::DIV;
  else if (str == "ENAME")               return AST::ID::ENAME;
  else if (str == "EQ")                  return AST::ID::EQ;
  else if (str == "FALSE")               return AST::ID::FALSE;
  else if (str == "IF")                  return AST::ID::IF;
  else if (str == "INT_LIT")             return AST::ID::INT_LIT;
  else if (str == "LET")                 return AST::ID::LET;
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
  else if (str == "i32_TEXP")            return AST::ID::i32_TEXP;
  else if (str == "REF_TEXP")            return AST::ID::REF_TEXP;
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

  if (id == AST::ID::ADD || id == AST::ID::SUB || id == AST::ID::MUL
  || id == AST::ID::DIV || id == AST::ID::EQ || id == AST::ID::NE) {
    auto n = reinterpret_cast<const BinopExp*>(nodePtr);
    ret.push_back(n->getLHS().upcast<AST>());
    ret.push_back(n->getRHS().upcast<AST>());
  } else if (id == AST::ID::ARRAY_INIT) {
    auto n = reinterpret_cast<const ArrayInitExp*>(nodePtr);
    ret.push_back(n->getSizeLit().upcast<AST>());
    ret.push_back(n->getInitializer().upcast<AST>());
  } else if (id == AST::ID::ARRAY_LIST) {
    auto n = reinterpret_cast<const ArrayListExp*>(nodePtr);
    ret.push_back(n->getContent().upcast<AST>());
  } else if (id == AST::ID::ARRAY_TEXP) {
    auto n = reinterpret_cast<const ArrayTypeExp*>(nodePtr);
    ret.push_back(n->getSizeLit().upcast<AST>());
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
  } else if (id == AST::ID::LET) {
    auto n = reinterpret_cast<const LetExp*>(nodePtr);
    ret.push_back(n->getBoundIdent().upcast<AST>());
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
  } else if (id == AST::ID::REF_EXP) {
    auto n = reinterpret_cast<const RefExp*>(nodePtr);
    ret.push_back(n->getInitializer().upcast<AST>());
  } else if (id == AST::ID::REF_TEXP) {
    auto n = reinterpret_cast<const RefTypeExp*>(nodePtr);
    ret.push_back(n->getPointeeType().upcast<AST>());
  } else if (id == AST::ID::RETURN) {
    auto n = reinterpret_cast<const ReturnExp*>(nodePtr);
    ret.push_back(n->getReturnee().upcast<AST>());
  } else if (id == AST::ID::STORE) {
    auto n = reinterpret_cast<const StoreExp*>(nodePtr);
    ret.push_back(n->getLHS().upcast<AST>());
    ret.push_back(n->getRHS().upcast<AST>());
  } else if (id == AST::ID::WREF_EXP) {
    auto n = reinterpret_cast<const WRefExp*>(nodePtr);
    ret.push_back(n->getInitializer().upcast<AST>());
  } else if (id == AST::ID::WREF_TEXP) {
    auto n = reinterpret_cast<const WRefTypeExp*>(nodePtr);
    ret.push_back(n->getPointeeType().upcast<AST>());
  }
  return ret;
}

#endif