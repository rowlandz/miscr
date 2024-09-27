#ifndef COMMON_AST
#define COMMON_AST

#include <string>
#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/Twine.h>
#include "common/Location.hpp"

class AST;
class Exp;
class Name;
void deleteAST(AST*);

/// @brief An AST node is any syntactic form that appears in source code. All
/// AST nodes have an `ID`, which is used to downcast to subclasses, and a
/// source code `location`.
///
/// The parser constructs the AST on the heap. After parsing, it is okay to
/// modify the AST as long as it does not result in allocation or deletion of
/// AST nodes. For example, fully-qualifying decl names only requires calling
/// the `set` method of the `Name` class.
///
/// Calling the deconstructor on an AST pointer `p` or the `deleteAST` function
/// should safely delete the entire subtree rooted at `p`. For this reason, the
/// AST should always truly be a tree structure to avoid double frees.
class AST {
public:
  enum ID : unsigned short {
    // expressions and statements
    ADD, ARRAY_INIT, ARRAY_LIST, ASCRIP, BLOCK, BOOL_LIT, CALL, DEC_LIT, DEREF,
    DIV, ENAME, EQ, GE, GT, IF, INDEX, INT_LIT, LE, LET, LT, MUL, NE, REF_EXP,
    RETURN, STORE, STRING_LIT, SUB,

    // declarations
    DATA, EXTERN_FUNC, FUNC, MODULE, NAMESPACE,

    // type expressions
    ARRAY_TEXP, BOOL_TEXP, f32_TEXP, f64_TEXP, i8_TEXP, i16_TEXP, i32_TEXP,
    i64_TEXP, NAME_TEXP, REF_TEXP, STR_TEXP, UNIT_TEXP, 

    // other
    DECLLIST, EXPLIST, NAME, PARAMLIST,
  };

private:
  Location location;
  ID id;

protected:
  AST(ID id, Location location) : id(id), location(location) {}
  ~AST() {};

public:
  ID getID() const { return id; }
  Location getLocation() const { return location; }

  bool isExp() const {
    switch (id) {
    case ADD: case ARRAY_INIT: case ARRAY_LIST: case ASCRIP: case BLOCK:
    case BOOL_LIT: case CALL: case DEC_LIT: case DEREF: case DIV: case ENAME:
    case EQ: case GE: case GT: case IF: case INDEX: case INT_LIT: case LE:
    case LET: case LT: case MUL: case NE: case REF_EXP: case RETURN: case STORE:
    case STRING_LIT: case SUB:
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
    BOOL, f32, f64, i8, i16, i32, i64, NAME, RREF, UNIT, WREF,

    // type constraints
    DECIMAL, NUMERIC, REF,

    // other
    NOTYPE,
  };
private:
  Type::ID id;
  union { unsigned int compileTime; Exp* runtime; } arraySize;
  TVar inner;
  const Name* _name;
  Type(Type::ID id) : arraySize({.compileTime=0}), id(id) {}
  Type(Type::ID id, TVar inner) : arraySize({.compileTime=0}), id(id),
    inner(inner) {}
  Type(Exp* arraySize, TVar of) : arraySize({.runtime = arraySize}), inner(of),
    id(ID::ARRAY_SART) {}
  Type(unsigned int arrSize, TVar of) : arraySize({.compileTime = arrSize}),
    inner(of), id(ID::ARRAY_SACT) {}
  Type(const Name* name)
    : arraySize({.compileTime=0}), id(ID::NAME), _name(name) {}
public:
  static Type array_sart(Exp* sz, TVar of) { return Type(sz, of); }
  static Type array_sact(unsigned int sz, TVar of) { return Type(sz, of); }
  static Type bool_() { return Type(ID::BOOL); }
  static Type f32() { return Type(ID::f32); }
  static Type f64() { return Type(ID::f64); }
  static Type i8() { return Type(ID::i8); }
  static Type i16() { return Type(ID::i16); }
  static Type i32() { return Type(ID::i32); }
  static Type i64() { return Type(ID::i64); }
  static Type name(const Name* n) { return Type(n); }
  static Type ref(TVar of) { return Type(ID::REF, of); }
  static Type rref(TVar of) { return Type(ID::RREF, of); }
  static Type unit() { return Type(ID::UNIT); }
  static Type wref(TVar of) { return Type(ID::WREF, of); }
  static Type decimal() { return Type(ID::DECIMAL); }
  static Type numeric() { return Type(ID::NUMERIC); }
  static Type notype() { return Type(ID::NOTYPE); }
  bool isNoType() const { return id == ID::NOTYPE; }
  bool isArrayType() const
    { return id == ID::ARRAY_SACT || id == ID::ARRAY_SART; }
  ID getID() const { return id; }
  TVar getInner() const { return inner; }
  unsigned int getCompileTimeArraySize() const { return arraySize.compileTime; }
  Exp* getRuntimeArraySize() const { return arraySize.runtime; }
  const Name* getName() const { return _name; }
};

/// @brief A qualified or unqualified name. Unqualified names are also called
/// identifiers. The stored string should always be one or more identifiers
/// separated by `::` and contain no whitespace. The source text may include
/// whitespace surrounding the `::` tokens. This whitespace is discarded by
/// the parser but still accounted for by the name's `location`.
class Name : public AST {
  std::string s;
public:
  Name(Location loc, std::string s) : AST(NAME, loc), s(s) {}
  static Name* downcast(AST* ast)
    { return ast->getID() == NAME ? static_cast<Name*>(ast) : nullptr; }
  llvm::StringRef asStringRef() const { return s; }
  /// Sets this name to `s`.
  void set(const llvm::Twine& s) { this->s = s.str(); }
};

////////////////////////////////////////////////////////////////////////////////
// TYPE EXPRESSIONS
////////////////////////////////////////////////////////////////////////////////

/// @brief A type that appears in the source text.
class TypeExp : public AST {
protected:
  TypeExp(ID id, Location loc) : AST(id, loc) {}
  ~TypeExp() {}
};

class PrimitiveTypeExp : public TypeExp {
  PrimitiveTypeExp(ID id, Location loc) : TypeExp(id, loc) {}
public:
  ~PrimitiveTypeExp() {}
  static PrimitiveTypeExp* newUnit(Location loc)
    { return new PrimitiveTypeExp(UNIT_TEXP, loc); }
  static PrimitiveTypeExp* newBool(Location loc)
    { return new PrimitiveTypeExp(BOOL_TEXP, loc); }
  static PrimitiveTypeExp* newI8(Location loc)
    { return new PrimitiveTypeExp(i8_TEXP, loc); }
  static PrimitiveTypeExp* newI16(Location loc)
    { return new PrimitiveTypeExp(i16_TEXP, loc); }
  static PrimitiveTypeExp* newI32(Location loc)
    { return new PrimitiveTypeExp(i32_TEXP, loc); }
  static PrimitiveTypeExp* newI64(Location loc)
    { return new PrimitiveTypeExp(i64_TEXP, loc); }
  static PrimitiveTypeExp* newF32(Location loc)
    { return new PrimitiveTypeExp(f32_TEXP, loc); }
  static PrimitiveTypeExp* newF64(Location loc)
    { return new PrimitiveTypeExp(f64_TEXP, loc); }
  static PrimitiveTypeExp* newStr(Location loc)
    { return new PrimitiveTypeExp(STR_TEXP, loc); }
  static PrimitiveTypeExp* downcast(AST* ast) {
    switch (ast->getID()) {
    case UNIT_TEXP: case BOOL_TEXP: case i8_TEXP: case i16_TEXP: case i32_TEXP:
    case i64_TEXP: case f32_TEXP: case f64_TEXP: case STR_TEXP:
      return static_cast<PrimitiveTypeExp*>(ast);
    default: return nullptr;
    }
  }
};

class NameTypeExp : public TypeExp {
  Name* name;
public:
  NameTypeExp(Name* name)
    : TypeExp(NAME_TEXP, name->getLocation()), name(name) {}
  ~NameTypeExp() { delete name; }
  static NameTypeExp* downcast(AST* ast) {
    return ast->getID() == NAME_TEXP ? static_cast<NameTypeExp*>(ast) : nullptr;
  }
  Name* getName() const { return name; }
};

/// @brief A read-only or writable reference type expression.
class RefTypeExp : public TypeExp {
  TypeExp* pointeeType;
  bool writable;
public:
  RefTypeExp(Location loc, TypeExp* pointeeType, bool writable)
    : TypeExp(REF_TEXP, loc), pointeeType(pointeeType), writable(writable) {}
  ~RefTypeExp() { deleteAST(pointeeType); }
  static RefTypeExp* downcast(AST* ast) {
    return ast->getID() == REF_TEXP ? static_cast<RefTypeExp*>(ast) : nullptr;
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
    : TypeExp(ARRAY_TEXP, loc), size(size), innerType(innerType) {}
  ~ArrayTypeExp()
    { deleteAST(reinterpret_cast<AST*>(size)); deleteAST(innerType); }
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
  ~Exp() {}
public:
  static Exp* downcast(AST* ast)
    { return ast->isExp() ? static_cast<Exp*>(ast) : nullptr; }
  TVar getTVar() const { return type; };
  void setTVar(TVar type) { this->type = type; }
};

/// @brief A linked-list of expressions.
class ExpList : public AST {
  llvm::SmallVector<Exp*, 4> exps;
public:
  ExpList(Location loc) : AST(EXPLIST, loc) {}
  ExpList(Location loc, llvm::SmallVector<Exp*, 4> exps)
    : AST(EXPLIST, loc), exps(exps) {}
  /// @brief Builds a new expression list from a head and tail. The tail is
  /// consumed and deleted and thus should not be used again. 
  ExpList(Location loc, Exp* head, ExpList* tail) : AST(EXPLIST, loc) {
    auto rest = tail->asArrayRef();
    exps.reserve(rest.size() + 1);
    exps.push_back(head);
    for (auto elem : rest) exps.push_back(elem);
    tail->exps.clear();
    delete tail;
  }
  ~ExpList() { for (Exp* e : exps) deleteAST(e); }
  static ExpList* downcast(AST* ast)
    { return ast->getID() == EXPLIST ? static_cast<ExpList*>(ast) : nullptr; }
  bool isEmpty() const { return exps.empty(); }
  bool nonEmpty() const { return !exps.empty(); }
  llvm::ArrayRef<Exp*> asArrayRef() const { return exps; }
};

/// @brief A boolean literal (`true` or `false`).
class BoolLit : public Exp {
  bool value;
public:
  BoolLit(Location loc, bool value) : Exp(BOOL_LIT, loc), value(value) {}
  static BoolLit* downcast(AST* ast)
    { return ast->getID() == BOOL_LIT ? static_cast<BoolLit*>(ast) : nullptr; }
  bool getValue() const { return value; }
};

/// @brief An integer literal.
class IntLit : public Exp {
  const char* ptr;
public:
  IntLit(Location loc, const char* ptr) : Exp(INT_LIT, loc), ptr(ptr) {}
  static IntLit* downcast(AST* ast)
    { return ast->getID() == INT_LIT ? static_cast<IntLit*>(ast) : nullptr; }
  long asLong() const
    { return atol(std::string(ptr, getLocation().sz).c_str()); }
  unsigned int asUint() const
    { return atoi(std::string(ptr, getLocation().sz).c_str()); }
  llvm::StringRef asStringRef()
    { return llvm::StringRef(ptr, getLocation().sz); }
};

/// @brief A decimal literal.
class DecimalLit : public Exp {
  const char* ptr;
public:
  DecimalLit(Location loc, const char* ptr) : Exp(DEC_LIT, loc), ptr(ptr) {}
  static DecimalLit* downcast(AST* ast) {
    return ast->getID() == DEC_LIT ? static_cast<DecimalLit*>(ast) : nullptr;
  }
  std::string asString() const { return std::string(ptr, getLocation().sz); }
  double asDouble() const
    { return atof(std::string(ptr, getLocation().sz).c_str()); }
};

/// @brief A string literal.
class StringLit : public Exp {
  const char* ptr;
public:
  /// Creates a string literal. The `loc` and `ptr` should point to the open
  /// quote of the string literal. 
  StringLit(Location loc, const char* ptr) : Exp(STRING_LIT, loc), ptr(ptr) {}
  static StringLit* downcast(AST* ast) {
    return ast->getID() == STRING_LIT ? static_cast<StringLit*>(ast) : nullptr;
  }
  /// Returns the contents of the string (excluding surrounding quotes).
  std::string asString() const
    { return std::string(ptr+1, getLocation().sz-2); }
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
  NameExp(Location loc, Name* name) : Exp(ENAME, loc), name(name) {}
  ~NameExp() { delete name; }
  static NameExp* downcast(AST* ast)
    { return ast->getID() == ENAME ? static_cast<NameExp*>(ast) : nullptr; }
  Name* getName() const { return name; }
};

/// @brief A binary operator expression.
class BinopExp : public Exp {
  Exp* lhs;
  Exp* rhs;
public:
  BinopExp(ID id, Location loc, Exp* lhs, Exp* rhs)
    : Exp(id, loc), lhs(lhs), rhs(rhs) {}
  ~BinopExp() { deleteAST(lhs); deleteAST(rhs); }
  static BinopExp* downcast(AST* ast)
    { return ast->isBinopExp() ? static_cast<BinopExp*>(ast) : nullptr; }
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
    : Exp(IF, loc), condExp(condExp), thenExp(thenExp), elseExp(elseExp) {}
  ~IfExp() { deleteAST(condExp); deleteAST(thenExp); deleteAST(elseExp); }
  static IfExp* downcast(AST* ast)
    { return ast->getID() == IF ? static_cast<IfExp*>(ast) : nullptr; }
  Exp* getCondExp() const { return condExp; }
  Exp* getThenExp() const { return thenExp; }
  Exp* getElseExp() const { return elseExp; }
};

/// @brief A block expression.
class BlockExp : public Exp {
  ExpList* statements;
public:
  BlockExp(Location loc, ExpList* stmts) : Exp(BLOCK, loc), statements(stmts) {}
  ~BlockExp() { delete statements; }
  static BlockExp* downcast(AST* ast)
    { return ast->getID() == BLOCK ? static_cast<BlockExp*>(ast) : nullptr; }
  ExpList* getStatements() const { return statements; }
};

/// @brief A function call expression.
class CallExp : public Exp {
  Name* function;
  ExpList* arguments;
public:
  CallExp(Location loc, Name* function, ExpList* arguments)
    : Exp(CALL, loc), function(function), arguments(arguments) {}
  ~CallExp() { delete function; delete arguments; }
  static CallExp* downcast(AST* ast)
    { return ast->getID() == CALL ? static_cast<CallExp*>(ast) : nullptr; }
  Name* getFunction() const { return function; }
  ExpList* getArguments() const { return arguments; }
};

/// @brief A type ascription expression.
class AscripExp : public Exp {
  Exp* ascriptee;
  TypeExp* ascripter;
public:
  AscripExp(Location loc, Exp* ascriptee, TypeExp* ascripter)
    : Exp(ASCRIP, loc), ascriptee(ascriptee), ascripter(ascripter) {}
  ~AscripExp() { deleteAST(ascriptee); deleteAST(ascripter); }
  static AscripExp* downcast(AST* ast)
    { return ast->getID() == ASCRIP ? static_cast<AscripExp*>(ast) : nullptr; }
  Exp* getAscriptee() const { return ascriptee; }
  TypeExp* getAscripter() const { return ascripter; }
};

/// @brief A let statement. The type ascription is optional.
class LetExp : public Exp {
  Name* boundIdent;
  TypeExp* ascrip;
  Exp* definition;
public:
  LetExp(Location loc, Name* ident, TypeExp* ascrip, Exp* def)
    : Exp(LET, loc), boundIdent(ident), ascrip(ascrip), definition(def) {}
  ~LetExp() { delete boundIdent; deleteAST(ascrip); deleteAST(definition); }
  static LetExp* downcast(AST* ast)
    { return ast->getID() == LET ? static_cast<LetExp*>(ast) : nullptr; }
  Name* getBoundIdent() const { return boundIdent; }
  TypeExp* getAscrip() const { return ascrip; }
  Exp* getDefinition() const { return definition; }
};

/// @brief A return statement.
class ReturnExp : public Exp {
  Exp* returnee;
public:
  ReturnExp(Location loc, Exp* returnee)
    : Exp(RETURN, loc), returnee(returnee) {}
  ~ReturnExp() { deleteAST(returnee); }
  static ReturnExp* downcast(AST* ast)
    { return ast->getID() == RETURN ? static_cast<ReturnExp*>(ast) : nullptr; }
  Exp* getReturnee() const { return returnee; }
};

/// @brief A store statement (e.g., `x := 42`).
class StoreExp : public Exp {
  Exp* lhs;
  Exp* rhs;
public:
  StoreExp(Location loc, Exp* lhs, Exp* rhs)
    : Exp(STORE, loc), lhs(lhs), rhs(rhs) {}
  ~StoreExp() { deleteAST(lhs); deleteAST(rhs); }
  static StoreExp* downcast(AST* ast)
    { return ast->getID() == STORE ? static_cast<StoreExp*>(ast) : nullptr; }
  Exp* getLHS() const { return lhs; }
  Exp* getRHS() const { return rhs; }
};

/// @brief An expression that allocates and initializes stack memory and returns
/// a reference to that memory (e.g., `&myexp` or `#myexp`).
class RefExp : public Exp {
  Exp* initializer;
  bool writable;
public:
  RefExp(Location loc, Exp* initializer, bool writable)
    : Exp(REF_EXP, loc), initializer(initializer), writable(writable) {}
  ~RefExp() { deleteAST(initializer); }
  static RefExp* downcast(AST* ast)
    { return (ast->getID() == REF_EXP) ? static_cast<RefExp*>(ast) : nullptr; }
  Exp* getInitializer() const { return initializer; }
  bool isWritable() const { return writable; }
};

/// @brief A dereference expression.
class DerefExp : public Exp {
  Exp* of;
public:
  DerefExp(Location loc, Exp* of) : Exp(DEREF, loc), of(of) {}
  ~DerefExp() { deleteAST(of); }
  static DerefExp* downcast(AST* ast)
    { return ast->getID() == DEREF ? static_cast<DerefExp*>(ast) : nullptr; }
  Exp* getOf() const { return of; }
};

/// @brief An array constructor that lists out the array contents.
/// e.g., `[42, x, y+1]`
class ArrayListExp : public Exp {
  ExpList* content;
public:
  ArrayListExp(Location loc, ExpList* content)
    : Exp(ARRAY_LIST, loc), content(content) {}
  ~ArrayListExp() { delete content; }
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
      : Exp(ARRAY_INIT, loc), size(size), initializer(initializer) {}
  ~ArrayInitExp() { deleteAST(size); deleteAST(initializer); }
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
  IndexExp(Location loc, Exp* base, Exp* index)
    : Exp(INDEX, loc), base(base), index(index) {}
  ~IndexExp() { deleteAST(base); deleteAST(index); }
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
  Decl(ID id, Location loc, Name* name) : AST(id, loc), name(name) {}
  ~Decl() { delete name; }
public:
  Name* getName() const { return name; }
};

/// @brief A list of declarations.
class DeclList : public AST {
  llvm::SmallVector<Decl*, 0> decls;
public:
  DeclList(Location loc, llvm::SmallVector<Decl*, 0> decls)
      : AST(DECLLIST, loc), decls(decls) {}
  ~DeclList() { for (Decl* d : decls) deleteAST(d); }
  static DeclList* downcast(AST* ast)
    { return ast->getID() == DECLLIST ? static_cast<DeclList*>(ast) : nullptr; }
  llvm::ArrayRef<Decl*> asArrayRef() const { return decls; }
};

class ModuleDecl : public Decl {
  DeclList* decls;
public:
  ModuleDecl(Location loc, Name* name, DeclList* decls)
      : Decl(MODULE, loc, name), decls(decls) {}
  ~ModuleDecl() { delete decls; }
  static ModuleDecl* downcast(AST* ast)
    { return ast->getID() == MODULE ? static_cast<ModuleDecl*>(ast) : nullptr; }
  DeclList* getDecls() const { return decls; }
};

class NamespaceDecl : public Decl {
  DeclList* decls;
public:
  NamespaceDecl(Location loc, Name* name, DeclList* decls)
    : Decl(NAMESPACE, loc, name), decls(decls) {}
  ~NamespaceDecl() { delete decls; }
  static NamespaceDecl* downcast(AST* ast) {
    return ast->getID() == NAMESPACE ?
           static_cast<NamespaceDecl*>(ast) : nullptr;
  }
  DeclList* getDecls() const { return decls; }
};

/// @brief A list of parameters in a function signature, or a list of
/// fields in a data type.
class ParamList : public AST {
  llvm::SmallVector<std::pair<Name*, TypeExp*>, 4> params;
public:
  ParamList(Location loc, llvm::SmallVector<std::pair<Name*, TypeExp*>, 4> ps)
    : AST(PARAMLIST, loc), params(ps) {}
  ~ParamList()
    { for (auto p : params) { delete p.first; deleteAST(p.second); } }
  static ParamList* downcast(AST* ast) {
    return ast->getID() == PARAMLIST ? static_cast<ParamList*>(ast) : nullptr;
  }
  llvm::ArrayRef<std::pair<Name*, TypeExp*>> asArrayRef() const
    { return params; }
};

/// @brief A function or extern function (FUNC/EXTERN_FUNC).
class FunctionDecl : public Decl {
  ParamList* parameters;
  TypeExp* returnType;
  Exp* body;
public:
  FunctionDecl(Location loc, Name* name, ParamList* params, TypeExp* returnType,
    Exp* body) : Decl(FUNC, loc, name), parameters(params),
    returnType(returnType), body(body) {}
  FunctionDecl(Location loc, Name* name, ParamList* params, TypeExp* returnType)
    : Decl(EXTERN_FUNC, loc, name), parameters(params), returnType(returnType),
    body(nullptr) {}
  ~FunctionDecl() { delete parameters; deleteAST(returnType); deleteAST(body); }
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
    : Decl(DATA, loc, name), fields(fields) {}
  ~DataDecl() { delete fields; }
  static DataDecl* downcast(AST* ast)
    { return ast->getID() == DATA ? static_cast<DataDecl*>(ast) : nullptr; }
  ParamList* getFields() const { return fields; }
};

////////////////////////////////////////////////////////////////////////////////
// DELETE AST
////////////////////////////////////////////////////////////////////////////////

/// @brief Safely deletes an AST node by first downcasting to the lowest type
/// in the inheritance hierarchy before calling the destructor. It is safe to
/// pass `nullptr` to this function.
void deleteAST(AST* _ast) {
       if (_ast == nullptr) {}
  else if (auto ast = Name::downcast(_ast)) delete ast;
  else if (auto ast = PrimitiveTypeExp::downcast(_ast)) delete ast;
  else if (auto ast = NameTypeExp::downcast(_ast)) delete ast;
  else if (auto ast = RefTypeExp::downcast(_ast)) delete ast;
  else if (auto ast = ArrayTypeExp::downcast(_ast)) delete ast;
  else if (auto ast = ExpList::downcast(_ast)) delete ast;
  else if (auto ast = BoolLit::downcast(_ast)) delete ast;
  else if (auto ast = IntLit::downcast(_ast)) delete ast;
  else if (auto ast = DecimalLit::downcast(_ast)) delete ast;
  else if (auto ast = StringLit::downcast(_ast)) delete ast;
  else if (auto ast = NameExp::downcast(_ast)) delete ast;
  else if (auto ast = BinopExp::downcast(_ast)) delete ast;
  else if (auto ast = IfExp::downcast(_ast)) delete ast;
  else if (auto ast = BlockExp::downcast(_ast)) delete ast;
  else if (auto ast = CallExp::downcast(_ast)) delete ast;
  else if (auto ast = AscripExp::downcast(_ast)) delete ast;
  else if (auto ast = LetExp::downcast(_ast)) delete ast;
  else if (auto ast = ReturnExp::downcast(_ast)) delete ast;
  else if (auto ast = StoreExp::downcast(_ast)) delete ast;
  else if (auto ast = RefExp::downcast(_ast)) delete ast;
  else if (auto ast = DerefExp::downcast(_ast)) delete ast;
  else if (auto ast = ArrayListExp::downcast(_ast)) delete ast;
  else if (auto ast = ArrayInitExp::downcast(_ast)) delete ast;
  else if (auto ast = IndexExp::downcast(_ast)) delete ast;
  else if (auto ast = DeclList::downcast(_ast)) delete ast;
  else if (auto ast = ModuleDecl::downcast(_ast)) delete ast;
  else if (auto ast = NamespaceDecl::downcast(_ast)) delete ast;
  else if (auto ast = ParamList::downcast(_ast)) delete ast;
  else if (auto ast = FunctionDecl::downcast(_ast)) delete ast;
  else if (auto ast = DataDecl::downcast(_ast)) delete ast;
  else llvm_unreachable("deleteAST case not supported!");
}

////////////////////////////////////////////////////////////////////////////////

const char* ASTIDToString(AST::ID nt) {
  switch (nt) {
  case AST::ID::ADD:                return "ADD";
  case AST::ID::ARRAY_INIT:         return "ARRAY_INIT";
  case AST::ID::ARRAY_LIST:         return "ARRAY_LIST";
  case AST::ID::ASCRIP:             return "ASCRIP";
  case AST::ID::BLOCK:              return "BLOCK";
  case AST::ID::BOOL_LIT:           return "BOOL_LIT";
  case AST::ID::CALL:               return "CALL";
  case AST::ID::DEC_LIT:            return "DEC_LIT";
  case AST::ID::DEREF:              return "DEREF";
  case AST::ID::DIV:                return "DIV";
  case AST::ID::ENAME:              return "ENAME";
  case AST::ID::EQ:                 return "EQ";
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
  case AST::ID::NAME_TEXP:          return "NAME_TEXP";
  case AST::ID::REF_TEXP:           return "REF_TEXP";
  case AST::ID::STR_TEXP:           return "STR_TEXP";
  case AST::ID::UNIT_TEXP:          return "UNIT_TEXP";
  
  case AST::ID::DECLLIST:           return "DECLLIST";
  case AST::ID::EXPLIST:            return "EXPLIST";
  case AST::ID::NAME:               return "NAME";
  case AST::ID::PARAMLIST:          return "PARAMLIST";
  }
}

AST::ID stringToASTID(const std::string& str) {
       if (str == "ADD")                 return AST::ID::ADD;
  else if (str == "ARRAY_INIT")          return AST::ID::ARRAY_INIT;
  else if (str == "ARRAY_LIST")          return AST::ID::ARRAY_LIST;
  else if (str == "ASCRIP")              return AST::ID::ASCRIP;
  else if (str == "BLOCK")               return AST::ID::BLOCK;
  else if (str == "BOOL_LIT")            return AST::ID::BOOL_LIT;
  else if (str == "CALL")                return AST::ID::CALL;
  else if (str == "DEC_LIT")             return AST::ID::DEC_LIT;
  else if (str == "DEREF")               return AST::ID::DEREF;
  else if (str == "DIV")                 return AST::ID::DIV;
  else if (str == "ENAME")               return AST::ID::ENAME;
  else if (str == "EQ")                  return AST::ID::EQ;
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
  else if (str == "NAME_TEXP")           return AST::ID::NAME_TEXP;
  else if (str == "REF_TEXP")            return AST::ID::REF_TEXP;
  else if (str == "STR_TEXP")            return AST::ID::STR_TEXP;
  else if (str == "UNIT_TEXP")           return AST::ID::UNIT_TEXP;
  
  else if (str == "DECLLIST")            return AST::ID::DECLLIST;
  else if (str == "EXPLIST")             return AST::ID::EXPLIST;
  else if (str == "NAME")                return AST::ID::NAME;
  else if (str == "PARAMLIST")           return AST::ID::PARAMLIST;

  else llvm_unreachable(("Invalid AST::ID string: " + str).c_str());
}

/// Returns the sub-ASTs of `node`.
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
  } else if (id == AST::ID::DECLLIST) {
    auto n = reinterpret_cast<const DeclList*>(ast);
    for (auto elem : n->asArrayRef()) ret.push_back(elem);
  } else if (id == AST::ID::DEREF) {
    auto n = reinterpret_cast<const DerefExp*>(ast);
    ret.push_back(n->getOf());
  } else if (id == AST::ID::ENAME) {
    auto n = reinterpret_cast<const NameExp*>(ast);
    ret.push_back(n->getName());
  } else if (id == AST::ID::EXPLIST) {
    auto n = reinterpret_cast<const ExpList*>(ast);
    for (auto elem : n->asArrayRef()) ret.push_back(elem);
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
  } else if (id == AST::ID::NAME_TEXP) {
    auto n = reinterpret_cast<const NameTypeExp*>(ast);
    ret.push_back(n->getName());
  } else if (id == AST::ID::PARAMLIST) {
    auto n = reinterpret_cast<const ParamList*>(ast);
    for (auto param : n->asArrayRef()) {
      ret.push_back(param.first);
      ret.push_back(param.second);
    }
  } else if (id == AST::ID::REF_EXP) {
    auto n = reinterpret_cast<const RefExp*>(ast);
    ret.push_back(n->getInitializer());
  } else if (id == AST::ID::REF_TEXP) {
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