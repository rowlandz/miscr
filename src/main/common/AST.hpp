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
    ADD, ASCRIP, BLOCK, BORROW, BOOL_LIT, CALL, CONSTR, DEC_LIT, DEREF, DIV,
    ENAME, EQ, GE, GT, IF, INDEX, INDEX_FIELD, INT_LIT, LE, LET, LT, MOVE, MUL,
    NE, PROJECT, REF_EXP, RETURN, STORE, STRING_LIT, SUB,

    // declarations
    DATA, EXTERN_FUNC, FUNC, MODULE, NAMESPACE,

    // type expressions
    ARRAY_TEXP, BOOL_TEXP, f32_TEXP, f64_TEXP, i8_TEXP, i16_TEXP, i32_TEXP,
    i64_TEXP, NAME_TEXP, REF_TEXP, STR_TEXP, UNIT_TEXP, 

    // other
    DECLLIST, EXPLIST, NAME, PARAMLIST,
  };

protected:
  Location location;
  ID id;

  AST(ID id, Location location) : id(id), location(location) {}
  ~AST() {};

public:
  ID getID() const { return id; }
  Location getLocation() const { return location; }

  bool isExp() const {
    switch (id) {
    case ADD: case ASCRIP: case BLOCK: case BOOL_LIT: case CALL: case CONSTR:
    case DEC_LIT: case DEREF: case DIV: case ENAME: case EQ: case GE: case GT:
    case IF: case INDEX: case INDEX_FIELD: case INT_LIT: case LE: case LET:
    case LT: case MUL: case NE: case PROJECT: case REF_EXP: case RETURN:
    case STORE: case STRING_LIT: case SUB:
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
    BOOL, BREF, f32, f64, i8, i16, i32, i64, NAME, OREF, UNIT,

    // type constraints
    DECIMAL, NUMERIC,

    // other
    NOTYPE,
  };
private:
  Type::ID id;
  TVar inner;
  const Name* _name;
  Type(Type::ID id) : id(id) {}
  Type(Type::ID id, TVar inner) : id(id),
    inner(inner) {}
  Type(const Name* name) : id(ID::NAME), _name(name) {}
public:
  static Type bool_() { return Type(ID::BOOL); }
  static Type bref(TVar of) { return Type(ID::BREF, of); }
  static Type f32() { return Type(ID::f32); }
  static Type f64() { return Type(ID::f64); }
  static Type i8() { return Type(ID::i8); }
  static Type i16() { return Type(ID::i16); }
  static Type i32() { return Type(ID::i32); }
  static Type i64() { return Type(ID::i64); }
  static Type name(const Name* n) { return Type(n); }
  static Type oref(TVar of) { return Type(ID::OREF, of); }
  static Type unit() { return Type(ID::UNIT); }
  static Type decimal() { return Type(ID::DECIMAL); }
  static Type numeric() { return Type(ID::NUMERIC); }
  static Type notype() { return Type(ID::NOTYPE); }
  bool isNoType() const { return id == ID::NOTYPE; }
  ID getID() const { return id; }
  TVar getInner() const { return inner; }
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

//============================================================================//
//=== TYPE EXPRESSIONS
//============================================================================//

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

/// @brief A borrowed or owned reference type expression.
class RefTypeExp : public TypeExp {
  TypeExp* pointeeType;
  bool owned;
public:
  RefTypeExp(Location loc, TypeExp* pointeeType, bool owned)
    : TypeExp(REF_TEXP, loc), pointeeType(pointeeType), owned(owned) {}
  ~RefTypeExp() { deleteAST(pointeeType); }
  static RefTypeExp* downcast(AST* ast) {
    return ast->getID() == REF_TEXP ? static_cast<RefTypeExp*>(ast) : nullptr;
  }
  TypeExp* getPointeeType() const { return pointeeType; }
  bool isBorrowed() const { return !owned; }
  bool isOwned() const { return owned; }
};

//============================================================================//
//=== EXPRESSIONS
//============================================================================//

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

/// @brief A function call or constructor invocation (CALL/CONSTR).
/// The parser will always generate CALL and the canonicalizer flips the
/// constructor calls to CONSTR.
class CallExp : public Exp {
  Name* function;
  ExpList* arguments;
public:
  CallExp(Location loc, Name* function, ExpList* arguments)
    : Exp(CALL, loc), function(function), arguments(arguments) {}
  ~CallExp() { delete function; delete arguments; }
  static CallExp* downcast(AST* ast) {
    return (ast->getID() == CALL || ast->getID() == CONSTR) ?
           static_cast<CallExp*>(ast) : nullptr;
  }
  Name* getFunction() const { return function; }
  ExpList* getArguments() const { return arguments; }
  void markAsConstr() { id = CONSTR; }
  bool isConstr() { return id == CONSTR; }
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
/// a reference to that memory (e.g., `&myexp`).
class RefExp : public Exp {
  Exp* initializer;
public:
  RefExp(Location loc, Exp* initializer)
    : Exp(REF_EXP, loc), initializer(initializer) {}
  ~RefExp() { deleteAST(initializer); }
  static RefExp* downcast(AST* ast)
    { return (ast->getID() == REF_EXP) ? static_cast<RefExp*>(ast) : nullptr; }
  Exp* getInitializer() const { return initializer; }
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

/// @brief An expression that extracts a field from a `data` value
/// (e.g., `bob.age`).
class ProjectExp : public Exp {
  Exp* base;
  Name* fieldName;
  std::string typeName;   // name of the datatype being indexed
public:
  ProjectExp(Location loc, Exp* base, Name* fieldName)
    : Exp(PROJECT, loc), base(base), fieldName(fieldName) {}
  ~ProjectExp() { deleteAST(base); delete fieldName; }
  static ProjectExp* downcast(AST* ast)
    {return ast->getID() == PROJECT ? static_cast<ProjectExp*>(ast) : nullptr;}
  Exp* getBase() const { return base; }
  Name* getFieldName() const { return fieldName; }

  /// Used the the unifier to set the name of the data decl.
  void setTypeName(llvm::StringRef name) { typeName = name.str(); }

  llvm::StringRef getTypeName() const { return typeName; }
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

/// @brief An expression that computes the address of a field of a data type.
/// (e.g., `bob[.age]`).
class IndexFieldExp : public Exp {
  Exp* base;
  Name* fieldName;
  std::string typeName;   // name of the datatype being indexed
public:
  IndexFieldExp(Location loc, Exp* base, Name* fieldName)
    : Exp(INDEX_FIELD, loc), base(base), fieldName(fieldName) {}
  ~IndexFieldExp() { deleteAST(base); delete fieldName; }
  static IndexFieldExp* downcast(AST* ast) {
    return ast->getID() == INDEX_FIELD ?
           static_cast<IndexFieldExp*>(ast) : nullptr;
  }
  Exp* getBase() const { return base; }
  Name* getFieldName() const { return fieldName; }

  /// Used the the unifier to set the name of the data decl being indexed.
  void setTypeName(llvm::StringRef name) { typeName = name.str(); }

  llvm::StringRef getTypeName() const { return typeName; }
};

/// @brief A borrow expression that returns a borrowed reference from an owned
/// reference.
class BorrowExp : public Exp {
  Exp* refExp;
public:
  BorrowExp(Location loc, Exp* refExp) : Exp(BORROW, loc), refExp(refExp) {}
  ~BorrowExp() { deleteAST(refExp); }
  static BorrowExp* downcast(AST* ast) {
    return ast->getID() == BORROW ? static_cast<BorrowExp*>(ast) : nullptr;
  }
  Exp* getRefExp() const { return refExp; }
};

/// @brief A borrow expression that returns a borrowed reference from an owned
/// reference.
class MoveExp : public Exp {
  Exp* refExp;
public:
  MoveExp(Location loc, Exp* refExp) : Exp(MOVE, loc), refExp(refExp) {}
  ~MoveExp() { deleteAST(refExp); }
  static MoveExp* downcast(AST* ast) {
    return ast->getID() == MOVE ? static_cast<MoveExp*>(ast) : nullptr;
  }
  Exp* getRefExp() const { return refExp; }
};

//============================================================================//
//=== DECLARATIONS
//============================================================================//

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

  /// Returns the type expression of the parameter named `paramName`, or
  /// `nullptr` if no such parameter exists.
  TypeExp* findParamType(llvm::StringRef paramName) const {
    for (auto param : params) {
      if (param.first->asStringRef() == paramName) return param.second;
    }
    return nullptr;
  }
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
  /// True if the not an `extern` function and `getBody` is not nullptr. 
  bool hasBody() const { return body != nullptr; }
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

//============================================================================//
//=== DELETE AST
//============================================================================//

/// @brief Safely deletes an AST node by first downcasting to the lowest type
/// in the inheritance hierarchy before calling the destructor. It is safe to
/// pass `nullptr` to this function.
void deleteAST(AST* _ast) {
       if (_ast == nullptr) {}
  else if (auto ast = Name::downcast(_ast)) delete ast;
  else if (auto ast = PrimitiveTypeExp::downcast(_ast)) delete ast;
  else if (auto ast = NameTypeExp::downcast(_ast)) delete ast;
  else if (auto ast = RefTypeExp::downcast(_ast)) delete ast;
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
  else if (auto ast = ProjectExp::downcast(_ast)) delete ast;
  else if (auto ast = IndexExp::downcast(_ast)) delete ast;
  else if (auto ast = IndexFieldExp::downcast(_ast)) delete ast;
  else if (auto ast = BorrowExp::downcast(_ast)) delete ast;
  else if (auto ast = MoveExp::downcast(_ast)) delete ast;
  else if (auto ast = DeclList::downcast(_ast)) delete ast;
  else if (auto ast = ModuleDecl::downcast(_ast)) delete ast;
  else if (auto ast = NamespaceDecl::downcast(_ast)) delete ast;
  else if (auto ast = ParamList::downcast(_ast)) delete ast;
  else if (auto ast = FunctionDecl::downcast(_ast)) delete ast;
  else if (auto ast = DataDecl::downcast(_ast)) delete ast;
  else llvm_unreachable("deleteAST case not supported!");
}

//============================================================================//

const char* ASTIDToString(AST::ID nt) {
  switch (nt) {
  case AST::ID::ADD:                return "ADD";
  case AST::ID::ASCRIP:             return "ASCRIP";
  case AST::ID::BLOCK:              return "BLOCK";
  case AST::ID::BORROW:             return "BORROW";
  case AST::ID::BOOL_LIT:           return "BOOL_LIT";
  case AST::ID::CALL:               return "CALL";
  case AST::ID::CONSTR:             return "CONSTR";
  case AST::ID::DEC_LIT:            return "DEC_LIT";
  case AST::ID::DEREF:              return "DEREF";
  case AST::ID::DIV:                return "DIV";
  case AST::ID::ENAME:              return "ENAME";
  case AST::ID::EQ:                 return "EQ";
  case AST::ID::IF:                 return "IF";
  case AST::ID::GE:                 return "GE";
  case AST::ID::GT:                 return "GT";
  case AST::ID::INDEX:              return "INDEX";
  case AST::ID::INDEX_FIELD:        return "INDEX_FIELD";
  case AST::ID::INT_LIT:            return "INT_LIT";
  case AST::ID::LE:                 return "LE";
  case AST::ID::LET:                return "LET";
  case AST::ID::LT:                 return "LT";
  case AST::ID::MOVE:               return "MOVE";
  case AST::ID::MUL:                return "MUL";
  case AST::ID::NE:                 return "NE";
  case AST::ID::PROJECT:            return "PROJECT";
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
  else if (str == "ASCRIP")              return AST::ID::ASCRIP;
  else if (str == "BLOCK")               return AST::ID::BLOCK;
  else if (str == "BORROW")              return AST::ID::BORROW;
  else if (str == "BOOL_LIT")            return AST::ID::BOOL_LIT;
  else if (str == "CALL")                return AST::ID::CALL;
  else if (str == "CONSTR")              return AST::ID::CONSTR;
  else if (str == "DEC_LIT")             return AST::ID::DEC_LIT;
  else if (str == "DEREF")               return AST::ID::DEREF;
  else if (str == "DIV")                 return AST::ID::DIV;
  else if (str == "ENAME")               return AST::ID::ENAME;
  else if (str == "EQ")                  return AST::ID::EQ;
  else if (str == "GE")                  return AST::ID::GE;
  else if (str == "GT")                  return AST::ID::GT;
  else if (str == "IF")                  return AST::ID::IF;
  else if (str == "INDEX")               return AST::ID::INDEX;
  else if (str == "INDEX_FIELD")         return AST::ID::INDEX_FIELD;
  else if (str == "INT_LIT")             return AST::ID::INT_LIT;
  else if (str == "LE")                  return AST::ID::LE;
  else if (str == "LET")                 return AST::ID::LET;
  else if (str == "LT")                  return AST::ID::LT;
  else if (str == "MOVE")                return AST::ID::MOVE;
  else if (str == "MUL")                 return AST::ID::MUL;
  else if (str == "NE")                  return AST::ID::NE;
  else if (str == "PROJECT")             return AST::ID::PROJECT;
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

/// Returns the children of this AST node.
llvm::SmallVector<AST*,4> getSubASTs(AST* _ast) {

  if (auto ast = BinopExp::downcast(_ast))
    return { ast->getLHS(), ast->getRHS() };
  if (auto ast = AscripExp::downcast(_ast))
    return { ast->getAscriptee(), ast->getAscripter() };
  if (auto ast = BlockExp::downcast(_ast))
    return { ast->getStatements() };
  if (auto ast = BorrowExp::downcast(_ast))
    return { ast->getRefExp() };
  if (auto ast = CallExp::downcast(_ast))
    return { ast->getFunction(), ast->getArguments() };
  if (auto ast = DataDecl::downcast(_ast))
    return { ast->getName(), ast->getFields() };
  if (auto ast = DeclList::downcast(_ast)) {
    llvm::SmallVector<AST*,4> ret;
    for (auto elem : ast->asArrayRef()) ret.push_back(elem);
    return ret;
  }
  if (auto ast = DerefExp::downcast(_ast))
    return { ast->getOf() };
  if (auto ast = NameExp::downcast(_ast))
    return { ast->getName() };
  if (auto ast = ExpList::downcast(_ast)) {
    llvm::SmallVector<AST*,4> ret;
    for (auto elem : ast->asArrayRef()) ret.push_back(elem);
    return ret;
  }
  if (auto ast = FunctionDecl::downcast(_ast)) {
    if (ast->hasBody())
      return { ast->getName(), ast->getParameters(), ast->getReturnType(),
          ast->getBody() };
    else
      return { ast->getName(), ast->getParameters(), ast->getReturnType() };
  }
  if (auto ast = IfExp::downcast(_ast))
    return { ast->getCondExp(), ast->getThenExp(), ast->getElseExp() };
  if (auto ast = IndexExp::downcast(_ast))
    return { ast->getBase(), ast->getIndex() };
  if (auto ast = IndexFieldExp::downcast(_ast))
    return { ast->getBase(), ast->getFieldName() };
  if (auto ast = LetExp::downcast(_ast)) {
    if (ast->getAscrip() != nullptr)
      return { ast->getBoundIdent(), ast->getAscrip(), ast->getDefinition() };
    else
      return { ast->getBoundIdent(), ast->getDefinition() };
  }
  if (auto ast = ModuleDecl::downcast(_ast))
    return { ast->getName(), ast->getDecls() };
  if (auto ast = MoveExp::downcast(_ast))
    return { ast->getRefExp() };
  if (auto ast = NamespaceDecl::downcast(_ast))
    return { ast->getName(), ast->getDecls() };
  if (auto ast = NameTypeExp::downcast(_ast))
    return { ast->getName() };
  if (auto ast = ParamList::downcast(_ast)) {
    llvm::SmallVector<AST*,4> ret;
    for (auto param : ast->asArrayRef()) {
      ret.push_back(param.first);
      ret.push_back(param.second);
    }
    return ret;
  }
  if (auto ast = ProjectExp::downcast(_ast))
    return { ast->getBase(), ast->getFieldName() };
  if (auto ast = RefExp::downcast(_ast))
    return { ast->getInitializer() };
  if (auto ast = RefTypeExp::downcast(_ast))
    return { ast->getPointeeType() };
  if (auto ast = ReturnExp::downcast(_ast))
    return { ast->getReturnee() };
  if (auto ast = StoreExp::downcast(_ast))
    return { ast->getLHS(), ast->getRHS() };
  return {};
}

#endif