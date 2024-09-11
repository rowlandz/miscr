#ifndef COMMON_AST
#define COMMON_AST

#include <cstdio>
#include <string>
#include "common/ASTContext.hpp"
#include "common/Location.hpp"

/// @brief Base class for all AST elements.
class AST {
public:
  enum ID : unsigned short {
    // expressions and statements
    ADD, ASCRIP, BLOCK, CALL, DEC_LIT, DIV, ENAME, EQ, FALSE, IF, INT_LIT, LET,
    MUL, NE, REF_EXP, RETURN, STORE, STRING_LIT, SUB, TRUE, WREF_EXP,

    // declarations
    EXTERN_FUNC, FUNC, MODULE, NAMESPACE,

    // types
    BOOL, DECIMAL, f32, f64, i8, i32, NUMERIC, REF, TYPEVAR, UNIT, WREF,

    // type expressions
    BOOL_TEXP, f32_TEXP, f64_TEXP, i8_TEXP, i32_TEXP, REF_TEXP, UNIT_TEXP,
    WREF_TEXP, 

    // other
    DECLLIST_CONS, DECLLIST_NIL, EXPLIST_CONS, EXPLIST_NIL, IDENT,
    PARAMLIST_CONS, PARAMLIST_NIL, QIDENT,
  };

private:
  ID id;

protected:
  AST(ID id) { this->id = id; }

public:
  ID getID() const { return id; }

  /// @brief Returns true if this AST can be downcast to `Type`. 
  bool isType() const {
    switch (id) {
    case BOOL: case DECIMAL: case f32: case f64: case i8: case i32: case REF:
    case TYPEVAR: case UNIT: case WREF: return true;
    default: return false;
    }
  }

  /// @brief Returns true if this AST can be downcast to `LocatedAST`. 
  bool isLocated() const { return !isType(); }

};

/// @brief The type of an expression or statement.
class Type : public AST {
protected:
  Type(ID id) : AST(id) {}
public:
  static Type mkUnit() { return Type(UNIT); }
  static Type mkBool() { return Type(BOOL); }
  static Type mkI8() { return Type(i8); }
  static Type mkI32() { return Type(i32); }
  static Type mkF32() { return Type(f32); }
  static Type mkF64() { return Type(f64); }
  static Type mkNumeric() { return Type(NUMERIC); }
  static Type mkDecimal() { return Type(DECIMAL); }
};

class TypeVar : public Type {
  int typeVarID;
public:
  TypeVar(int typeVarID) : Type(TYPEVAR) {
    this->typeVarID = typeVarID;
  }
  int getTypeVarID() const { return typeVarID; }
};

class RefType : public Type {
  Addr<Type> what;
public:
  RefType(Addr<Type> what) : Type(REF) {
    this->what = what;
  }
};

class WRefType : public Type {
  Addr<Type> what;
public:
  WRefType(Addr<Type> what) : Type(WREF) {
    this->what = what;
  }
};

/// @brief An AST element that has a location in the source text.
class LocatedAST : public AST {
  unsigned long unused0 : 48;
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

/// @brief A qualified or unqualified identifier (QIDENT or IDENT).
class Name : public LocatedAST {
protected:
  Name(ID id, Location loc) : LocatedAST(id, loc) {}
};

/// @brief An unqualified name.
class Ident : Name {
  const char* ptr;
public:
  Ident(Location loc, const char* ptr) : Name(IDENT, loc) {
    this->ptr = ptr;
  }
  std::string asString() { return std::string(ptr, getLocation().sz); }
};

/// @brief A name with at least one qualifier (the head).
class QIdent : Name {
  Addr<Ident> head;
  Addr<Name> tail;
public:
  QIdent(Location loc, Addr<Ident> head, Addr<Name> tail) : Name(QIDENT, loc) {
    this->head = head; this->tail = tail;
  }
  Addr<Ident> getHead() const { return head; }
  Addr<Name> getTail() const { return tail; }
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


/// @brief An expression or statement (currently there is no distinction).
class Exp : public LocatedAST {
protected:
  Addr<Type> type;
  Exp(ID id, Location loc) : LocatedAST(id, loc) {}
public:
  Addr<Type> getType() const { return type; };
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

/// @brief A declaration.
class Decl : public LocatedAST {
protected:
  Addr<Name> name;
  Decl(ID id, Location loc, Addr<Name> name) : LocatedAST(id, loc) {
    this->name = name;
  }
public:
  Addr<Name> getName() const { return name; }
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
  Addr<ParamList> getParameters() const { return parameters; }
  Addr<TypeExp> getReturnType() const { return returnType; }
  Addr<Exp> getBody() const { return body; }
};


bool isExpAST(AST::ID ty) {
  switch (ty) {
    case AST::ID::ENAME:
    case AST::ID::INT_LIT:
    case AST::ID::DEC_LIT:
    case AST::ID::STRING_LIT:
    case AST::ID::TRUE:
    case AST::ID::FALSE:
    case AST::ID::ADD:
    case AST::ID::SUB:
    case AST::ID::MUL:
    case AST::ID::DIV:
    case AST::ID::EQ:
    case AST::ID::NE:
    case AST::ID::REF_EXP:
    case AST::ID::WREF_EXP:
    case AST::ID::IF:
    case AST::ID::BLOCK:
    case AST::ID::CALL:
    case AST::ID::ASCRIP:
      return true;
    default: return false;
  }
}

const char* ASTIDToString(AST::ID nt) {
  switch (nt) {
  case AST::ID::ADD:                return "ADD";
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

  case AST::ID::BOOL:               return "BOOL";
  case AST::ID::DECIMAL:            return "DECIMAL";
  case AST::ID::f32:                return "f32";
  case AST::ID::f64:                return "f64";
  case AST::ID::i8:                 return "i8";
  case AST::ID::i32:                return "i32";
  case AST::ID::NUMERIC:            return "NUMERIC";
  case AST::ID::REF:                return "REF";
  case AST::ID::TYPEVAR:            return "TYPEVAR";
  case AST::ID::UNIT:               return "UNIT";
  case AST::ID::WREF:               return "WREF";
  
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
  case AST::ID::IDENT:              return "IDENT";
  case AST::ID::PARAMLIST_CONS:     return "PARAMLIST_CONS";
  case AST::ID::PARAMLIST_NIL:      return "PARAMLIST_NIL";
  case AST::ID::QIDENT:             return "QIDENT";
  }
}

AST::ID stringToASTID(const std::string& str) {
       if (str == "ADD")                 return AST::ID::ADD;
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
  else if (str == "BOOL")                return AST::ID::BOOL;
  else if (str == "DECIMAL")             return AST::ID::DECIMAL;
  else if (str == "f32")                 return AST::ID::f32;
  else if (str == "f64")                 return AST::ID::f64;
  else if (str == "i8")                  return AST::ID::i8;
  else if (str == "i32")                 return AST::ID::i32;
  else if (str == "NUMERIC")             return AST::ID::NUMERIC;
  else if (str == "REF")                 return AST::ID::REF;
  else if (str == "TYPEVAR")             return AST::ID::TYPEVAR;
  else if (str == "UNIT")                return AST::ID::UNIT;
  else if (str == "WREF")                return AST::ID::WREF;
  
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
  else if (str == "IDENT")               return AST::ID::IDENT;
  else if (str == "PARAMLIST_CONS")      return AST::ID::PARAMLIST_CONS;
  else if (str == "PARAMLIST_NIL")       return AST::ID::PARAMLIST_NIL;
  else if (str == "QIDENT")              return AST::ID::QIDENT;

  else { printf("Invalid AST::ID string: %s\n", str.c_str()); exit(1); }
}

#define CAST(newtype, value) reinterpret_cast<newtype*>(&value)

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
  } else if (id == AST::ID::REF_EXP) {
    auto n = reinterpret_cast<const RefExp*>(nodePtr);
    ret.push_back(n->getInitializer().upcast<AST>());
  } else if (id == AST::ID::WREF_EXP) {
    auto n = reinterpret_cast<const WRefExp*>(nodePtr);
    ret.push_back(n->getInitializer().upcast<AST>());
  } else if (id == AST::ID::MODULE) {
    auto n = reinterpret_cast<const ModuleDecl*>(nodePtr);
    ret.push_back(n->getName().upcast<AST>());
    ret.push_back(n->getDecls().upcast<AST>());
  } else if (id == AST::ID::NAMESPACE) {
    auto n = reinterpret_cast<const NamespaceDecl*>(nodePtr);
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
  } else if (id == AST::ID::WREF_TEXP) {
    auto n = reinterpret_cast<const WRefTypeExp*>(nodePtr);
    ret.push_back(n->getPointeeType().upcast<AST>());
  }
  return ret;
}

#endif