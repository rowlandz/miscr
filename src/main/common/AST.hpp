#ifndef COMMON_AST
#define COMMON_AST

#include <string>
#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/Twine.h>
#include "common/Location.hpp"
#include "common/Type.hpp"

class Name;

/// @brief An AST node is any syntactic form that appears in source code. All
/// AST nodes have an `ID`, which is used to downcast to subclasses, and a
/// source code `location`.
///
/// The parser constructs the AST on the heap. After parsing, it is okay to
/// modify the AST as long as it does not result in allocation or deletion of
/// AST nodes. For example, fully-qualifying decl names only requires calling
/// the `set` method of the `Name` class.
///
/// Syntax trees should always truely be tree structures, not just directed
/// graphs, otherwise the deleteRecursive() function will result in double-free
/// errors.
class AST {
public:

  /// @brief There is one ID for every final descendant class of AST.
  enum ID : unsigned char {

    // expressions and statements
    ASCRIP, BINOP_EXP, BLOCK, BORROW, BOOL_LIT, CALL, DEC_LIT, DEREF, ENAME,
    IF, INDEX, INT_LIT, LET, MOVE, PROJECT, REF_EXP, RETURN, STORE, STRING_LIT,
    UNOP_EXP, WHILE,

    // declarations
    DATA, FUNC, MODULE,

    // type expressions
    NAME_TEXP, PRIMITIVE_TEXP, REF_TEXP,

    // other
    DECLLIST, EXPLIST, NAME, PARAMLIST,
  };

protected:
  Location location;
  AST(ID id, Location location) : id(id), location(location) {}
  ~AST() {};

public:

  /// @brief Determines what type of AST node this is. Though this field can
  /// be inspected directly, it is usually better to use the static `downcast`
  /// functions in AST-derived classes.
  const ID id;

  static const char* IDToString(AST::ID);

  /// @brief Returns `id` as a string matching its name in the AST::ID enum.
  const char* getIDAsString() const { return IDToString(id); }

  /// @brief Returns where this syntax element appears in the source code. 
  Location getLocation() const { return location; }

  /// @brief Returns the child syntax nodes of this AST node. 
  llvm::SmallVector<AST*> getASTChildren();

  /// @brief Recursively deletes this whole `new`-allocated syntax tree.
  void deleteRecursive() {
    for (AST* child : getASTChildren()) child->deleteRecursive();
    delete this;
  }
};

/// @brief A qualified or unqualified name. Unqualified names are also called
/// identifiers. The stored string should always be one or more identifiers
/// separated by `::` and contain no whitespace (though whitespace is allowed
/// to surround the `::` tokens in the source code itself).
class Name : public AST {
  std::string s;
public:
  Name(Location loc, llvm::StringRef s) : AST(NAME, loc), s(s) {}
  static Name* downcast(AST* ast)
    { return ast->id == NAME ? static_cast<Name*>(ast) : nullptr; }
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

/// @brief A type expression representing a primitive type.
class PrimitiveTypeExp : public TypeExp {
public:

  enum Kind : unsigned char { BOOL, f32, f64, i8, i16, i32, i64, UNIT };

  /// @brief The kind of primitive type this represents.
  const Kind kind;

  PrimitiveTypeExp(Location loc, Kind kind)
    : TypeExp(PRIMITIVE_TEXP, loc), kind(kind) {}

  static PrimitiveTypeExp* downcast(AST* ast) {
    return ast->id == PRIMITIVE_TEXP ?
           static_cast<PrimitiveTypeExp*>(ast) : nullptr;
  }

  /// @brief Returns the kind as a string matching its syntax in source code. 
  const char* getKindAsString() const {
    switch (kind) {
    case BOOL:   return "bool";
    case f32:    return "f32";
    case f64:    return "f64";
    case i8:     return "i8";
    case i16:    return "i16";
    case i32:    return "i32";
    case i64:    return "i64";
    case UNIT:   return "unit";
    }
    llvm_unreachable("PrimitiveTypeExp::getKindAsString unhandled switch case");
    return nullptr;
  }
};

/// @brief A Name used as a type expression.
class NameTypeExp : public TypeExp {
  Name* name;
public:
  NameTypeExp(Name* name)
    : TypeExp(NAME_TEXP, name->getLocation()), name(name) {}
  static NameTypeExp* downcast(AST* ast) {
    return ast->id == NAME_TEXP ? static_cast<NameTypeExp*>(ast) : nullptr;
  }
  Name* getName() const { return name; }
};

/// @brief A borrowed or unique reference type expression.
class RefTypeExp : public TypeExp {
  TypeExp* pointeeType;
  bool unique;
public:
  RefTypeExp(Location loc, TypeExp* pointeeType, bool unique)
    : TypeExp(REF_TEXP, loc), pointeeType(pointeeType), unique(unique) {}
  static RefTypeExp* downcast(AST* ast)
    { return ast->id == REF_TEXP ? static_cast<RefTypeExp*>(ast) : nullptr; }
  TypeExp* getPointeeType() const { return pointeeType; }
  bool isBorrowed() const { return !unique; }
  bool isUnique() const { return unique; }
};

//============================================================================//
//=== EXPRESSIONS
//============================================================================//

/// @brief An expression or statement (currently there is no distinction).
class Exp : public AST {
protected:
  Type* type;
  Exp(ID id, Location loc) : AST(id, loc), type(nullptr) {}
  ~Exp() {}
public:
  static Exp* downcast(AST* ast) {
    switch (ast->id) {
    case ASCRIP: case BINOP_EXP: case BLOCK: case BOOL_LIT: case BORROW:
    case CALL: case DEC_LIT: case DEREF: case ENAME: case IF: case INDEX:
    case INT_LIT: case LET: case MOVE: case PROJECT: case REF_EXP: case RETURN:
    case STORE: case STRING_LIT: case UNOP_EXP: case WHILE:
      return static_cast<Exp*>(ast);
    default: return nullptr;
    }
  }
  Type* getType() const { return type; }
  void setType(Type* type) { this->type = type; }
};

/// @brief A list of expressions (e.g., arguments in a function call or
/// statements in a block).
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

  static ExpList* downcast(AST* ast)
    { return ast->id == EXPLIST ? static_cast<ExpList*>(ast) : nullptr; }
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
    { return ast->id == BOOL_LIT ? static_cast<BoolLit*>(ast) : nullptr; }
  bool getValue() const { return value; }
};

/// @brief An integer literal.
class IntLit : public Exp {
  const char* ptr;
public:
  IntLit(Location loc, const char* ptr) : Exp(INT_LIT, loc), ptr(ptr) {}
  static IntLit* downcast(AST* ast)
    { return ast->id == INT_LIT ? static_cast<IntLit*>(ast) : nullptr; }
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
  static DecimalLit* downcast(AST* ast)
    { return ast->id == DEC_LIT ? static_cast<DecimalLit*>(ast) : nullptr; }
  std::string asString() const { return std::string(ptr, getLocation().sz); }
  double asDouble() const
    { return atof(std::string(ptr, getLocation().sz).c_str()); }
};

/// @brief A string literal.
class StringLit : public Exp {
  const char* ptr;
public:

  /// @brief Creates a string literal. The @p loc and @p ptr should point to
  /// the open quote of the string literal. 
  StringLit(Location loc, const char* ptr) : Exp(STRING_LIT, loc), ptr(ptr) {}
  static StringLit* downcast(AST* ast)
    { return ast->id == STRING_LIT ? static_cast<StringLit*>(ast) : nullptr; }

  /// @brief Returns the contents of the string (excluding surrounding quotes).
  std::string asString() const
    { return std::string(ptr+1, getLocation().sz-2); }

  /// @brief Returns the contents of the string with escape sequences processed. 
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
  NameExp(Name* name) : Exp(ENAME, name->getLocation()), name(name) {}
  static NameExp* downcast(AST* ast)
    { return ast->id == ENAME ? static_cast<NameExp*>(ast) : nullptr; }
  Name* getName() const { return name; }
};

/// @brief An arithmetic or logical prefix operator expression.
class UnopExp : public Exp {
public:
  enum Unop { NOT, NEG };
private:
  Unop unop;
  Exp* inner;
public:
  UnopExp(Location loc, Unop unop, Exp* inner)
    : Exp(UNOP_EXP, loc), unop(unop), inner(inner) {}
  static UnopExp* downcast(AST* ast)
    { return ast->id == UNOP_EXP ? static_cast<UnopExp*>(ast) : nullptr; }
  Unop getUnop() const { return unop; }
  Exp* getInner() const { return inner; }

  /// @brief Returns the unary operator as a string matching its appearance
  /// in the Unop enum.
  const char* getUnopAsEnumString() const {
    switch (unop) {
    case NOT:   return "NOT";
    case NEG:   return "NEG";
    }
    llvm_unreachable("UnopExp::getUnopAsEnumString() unhandled switch case");
    return nullptr;
  }
};

/// @brief An arithmetic, logical, or comparison binary operator expression.
class BinopExp : public Exp {
public:
  enum Binop { ADD, AND, DIV, EQ, GE, GT, LE, LT, MOD, MUL, NE, OR, SUB };
private:
  Binop binop;
  Exp* lhs;
  Exp* rhs;
public:
  BinopExp(Location loc, Binop binop, Exp* lhs, Exp* rhs)
    : Exp(BINOP_EXP, loc), binop(binop), lhs(lhs), rhs(rhs) {}
  static BinopExp* downcast(AST* ast)
    { return ast->id == BINOP_EXP ? static_cast<BinopExp*>(ast) : nullptr; }
  Binop getBinop() const { return binop; }
  Exp* getLHS() const { return lhs; }
  Exp* getRHS() const { return rhs; }

  /// @brief Returns the binary operator as a string matching its appearance
  /// in the Binop enum.
  const char* getBinopAsEnumString() const {
    switch (binop) {
    case ADD:   return "ADD";
    case AND:   return "AND";
    case DIV:   return "DIV";
    case EQ:    return "EQ";
    case GE:    return "GE";
    case GT:    return "GT";
    case LE:    return "LE";
    case LT:    return "LT";
    case MOD:   return "MOD";
    case MUL:   return "MUL";
    case NE:    return "NE";
    case OR:    return "OR";
    case SUB:   return "SUB";
    }
    llvm_unreachable("BinopExp::getBinopAsEnumString() unhandled switch case");
    return nullptr;
  }
};

/// @brief An `if` or `if-else` expression/statement. The `elseExp` could be
/// nullptr.
class IfExp : public Exp {
  Exp* condExp;
  Exp* thenExp;
  Exp* elseExp;
public:
  IfExp(Location loc, Exp* condExp, Exp* thenExp, Exp* elseExp)
    : Exp(IF, loc), condExp(condExp), thenExp(thenExp), elseExp(elseExp) {}
  static IfExp* downcast(AST* ast)
    { return ast->id == IF ? static_cast<IfExp*>(ast) : nullptr; }
  Exp* getCondExp() const { return condExp; }
  Exp* getThenExp() const { return thenExp; }
  Exp* getElseExp() const { return elseExp; }
};

/// @brief A while-loop
class WhileExp : public Exp {
  Exp* cond;
  Exp* body;
public:
  WhileExp(Location loc, Exp* cond, Exp* body)
    : Exp(WHILE, loc), cond(cond), body(body) {}
  static WhileExp* downcast(AST* ast)
    { return ast->id == WHILE ? static_cast<WhileExp*>(ast) : nullptr; }
  Exp* getCond() const { return cond; }
  Exp* getBody() const { return body; }
};

/// @brief A block expression.
class BlockExp : public Exp {
  ExpList* statements;
public:
  BlockExp(Location loc, ExpList* stmts) : Exp(BLOCK, loc), statements(stmts) {}
  static BlockExp* downcast(AST* ast)
    { return ast->id == BLOCK ? static_cast<BlockExp*>(ast) : nullptr; }
  ExpList* getStatements() const { return statements; }
};

/// @brief A function call or constructor invocation. The canonicalizer
/// distinguishes the two using markAsConstr().
class CallExp : public Exp {
  bool _isConstr;
  Name* function;
  ExpList* arguments;
public:
  CallExp(Location loc, Name* function, ExpList* arguments) : Exp(CALL, loc),
    _isConstr(false), function(function), arguments(arguments) {}
  static CallExp* downcast(AST* ast)
    { return (ast->id == CALL) ? static_cast<CallExp*>(ast) : nullptr; }
  Name* getFunction() const { return function; }
  ExpList* getArguments() const { return arguments; }
  void markAsConstr() { _isConstr = true; }
  bool isConstr() { return _isConstr; }
};

/// @brief A type ascription expression.
class AscripExp : public Exp {
  Exp* ascriptee;
  TypeExp* ascripter;
public:
  AscripExp(Location loc, Exp* ascriptee, TypeExp* ascripter)
    : Exp(ASCRIP, loc), ascriptee(ascriptee), ascripter(ascripter) {}
  static AscripExp* downcast(AST* ast)
    { return ast->id == ASCRIP ? static_cast<AscripExp*>(ast) : nullptr; }
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
  static LetExp* downcast(AST* ast)
    { return ast->id == LET ? static_cast<LetExp*>(ast) : nullptr; }
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
  static ReturnExp* downcast(AST* ast)
    { return ast->id == RETURN ? static_cast<ReturnExp*>(ast) : nullptr; }
  Exp* getReturnee() const { return returnee; }
};

/// @brief A store statement (e.g., `x := 42`).
class StoreExp : public Exp {
  Exp* lhs;
  Exp* rhs;
public:
  StoreExp(Location loc, Exp* lhs, Exp* rhs)
    : Exp(STORE, loc), lhs(lhs), rhs(rhs) {}
  static StoreExp* downcast(AST* ast)
    { return ast->id == STORE ? static_cast<StoreExp*>(ast) : nullptr; }
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
  static RefExp* downcast(AST* ast)
    { return (ast->id == REF_EXP) ? static_cast<RefExp*>(ast) : nullptr; }
  Exp* getInitializer() const { return initializer; }
};

/// @brief A dereference expression.
class DerefExp : public Exp {
  Exp* of;
public:
  DerefExp(Location loc, Exp* of) : Exp(DEREF, loc), of(of) {}
  static DerefExp* downcast(AST* ast)
    { return ast->id == DEREF ? static_cast<DerefExp*>(ast) : nullptr; }
  Exp* getOf() const { return of; }
};

/// @brief An expression that accesses a field of a `data` value.
///
/// There are three syntaxes for projection with slightly different meanings:
///   - `base.field`   -- converts the data value to the field value
///   - `base[.field]` -- converts a _reference_ to a data value into a
///                       _reference_ to the field value
///   - `base->field`  -- converts a _reference_ to a data value into the field
///                       value
///
/// The arrow kind is equivalent to `base[.field]!` and `base!.field`.
class ProjectExp : public Exp {
public:

  /// @brief `base.field`, `base[.field]` or `base->field` syntax.
  enum Kind : unsigned char { DOT, BRACKETS, ARROW };

private:
  Exp* base;
  Name* fieldName;
  Kind kind;
  std::string typeName;   // name of the datatype being indexed

public:
  ProjectExp(Location loc, Exp* base, Name* fieldName, Kind kind)
    : Exp(PROJECT, loc), base(base), fieldName(fieldName), kind(kind) {}
  static ProjectExp* downcast(AST* ast)
    { return ast->id == PROJECT ? static_cast<ProjectExp*>(ast) : nullptr; }
  Exp* getBase() const { return base; }
  Name* getFieldName() const { return fieldName; }
  Kind getKind() const { return kind; }

  /// @brief Used in the unifier to set the name of the data decl.
  void setTypeName(llvm::StringRef name) { typeName = name.str(); }

  /// @brief Returns the fully-qualified name of the data type being projected.
  /// This is only valid after the type checker sets this value. 
  llvm::StringRef getTypeName() const { return typeName; }

  /// @brief Returns the kind as a string matching its name in the Kind enum.
  const char* getKindAsEnumString() const {
    switch (kind) {
    case DOT:        return "DOT";
    case BRACKETS:   return "BRACKETS";
    case ARROW:      return "ARROW";
    }
    llvm_unreachable("ProjectExp::getKindAsEnumString() unhandled switch case");
    return nullptr;
  }
};

/// @brief An expression that calculates an address from a base reference and
/// offset (e.g., `myarrayref[3]`).
class IndexExp : public Exp {
  Exp* base;
  Exp* index;
public:
  IndexExp(Location loc, Exp* base, Exp* index)
    : Exp(INDEX, loc), base(base), index(index) {}
  static IndexExp* downcast(AST* ast)
    { return ast->id == INDEX ? static_cast<IndexExp*>(ast) : nullptr; }
  Exp* getBase() const { return base; }
  Exp* getIndex() const { return index; }
};

/// @brief A borrow expression that returns a borrowed reference from an owned
/// reference.
class BorrowExp : public Exp {
  Exp* refExp;
public:
  BorrowExp(Location loc, Exp* refExp) : Exp(BORROW, loc), refExp(refExp) {}
  static BorrowExp* downcast(AST* ast)
    { return ast->id == BORROW ? static_cast<BorrowExp*>(ast) : nullptr; }
  Exp* getRefExp() const { return refExp; }
};

/// @brief An "operator" of type `&#T -> #T` that extracts an owned reference
/// from behind a borrow.
class MoveExp : public Exp {
  Exp* refExp;
public:
  MoveExp(Location loc, Exp* refExp) : Exp(MOVE, loc), refExp(refExp) {}
  static MoveExp* downcast(AST* ast)
    { return ast->id == MOVE ? static_cast<MoveExp*>(ast) : nullptr; }
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
  ~Decl() {}
public:
  Name* getName() const { return name; }
};

/// @brief A list of declarations.
class DeclList : public AST {
  llvm::SmallVector<Decl*, 0> decls;
public:
  DeclList(Location loc, llvm::SmallVector<Decl*, 0> decls)
      : AST(DECLLIST, loc), decls(decls) {}
  static DeclList* downcast(AST* ast)
    { return ast->id == DECLLIST ? static_cast<DeclList*>(ast) : nullptr; }
  llvm::ArrayRef<Decl*> asArrayRef() const { return decls; }
};

/// @brief A module, the basic unit of MiSCR's namespace hierarchy.
class ModuleDecl : public Decl {
  DeclList* decls;
public:
  ModuleDecl(Location loc, Name* name, DeclList* decls)
      : Decl(MODULE, loc, name), decls(decls) {}
  static ModuleDecl* downcast(AST* ast)
    { return ast->id == MODULE ? static_cast<ModuleDecl*>(ast) : nullptr; }
  DeclList* getDecls() const { return decls; }
};

/// @brief A list of parameters in a function signature, or a list of
/// fields in a data type.
class ParamList : public AST {
  llvm::SmallVector<std::pair<Name*, TypeExp*>, 4> params;
public:
  ParamList(Location loc, llvm::SmallVector<std::pair<Name*, TypeExp*>, 4> ps)
    : AST(PARAMLIST, loc), params(ps) {}
  static ParamList* downcast(AST* ast)
    { return ast->id == PARAMLIST ? static_cast<ParamList*>(ast) : nullptr; }
  llvm::ArrayRef<std::pair<Name*, TypeExp*>> asArrayRef() const
    { return params; }

  /// Returns the type expression of the parameter named @p paramName, or
  /// nullptr if no such parameter exists.
  TypeExp* findParamType(llvm::StringRef paramName) const {
    for (auto param : params) {
      if (param.first->asStringRef() == paramName) return param.second;
    }
    return nullptr;
  }
};

/// @brief A function or extern function.
///
/// If the function is variadic, then calls to the function can accept zero or
/// more additional arguments of any type after its specified parameters.
class FunctionDecl : public Decl {
  ParamList* parameters;
  bool variadic;
  TypeExp* returnType;
  Exp* body;
public:
  FunctionDecl(Location loc, Name* name, ParamList* params, TypeExp* returnType,
    Exp* body = nullptr, bool variadic = false) : Decl(FUNC, loc, name),
    parameters(params), returnType(returnType), body(body), variadic(variadic){}
  static FunctionDecl* downcast(AST* ast)
    { return ast->id == FUNC ? static_cast<FunctionDecl*>(ast) : nullptr; }
  ParamList* getParameters() const { return parameters; }
  TypeExp* getReturnType() const { return returnType; }
  Exp* getBody() const { return body; }
  bool isVariadic() const { return variadic; }

  /// @brief Returns true iff this function has no body.
  bool isExtern() const { return body == nullptr; }

  /// @brief True iff not an `extern` function and getBody() is not nullptr.
  bool hasBody() const { return body != nullptr; }
};

/// @brief A data type declaration.
class DataDecl : public Decl {
  ParamList* fields;
public:
  DataDecl(Location loc, Name* name, ParamList* fields)
    : Decl(DATA, loc, name), fields(fields) {}
  static DataDecl* downcast(AST* ast)
    { return ast->id == DATA ? static_cast<DataDecl*>(ast) : nullptr; }
  ParamList* getFields() const { return fields; }
};

//============================================================================//

llvm::SmallVector<AST*> AST::getASTChildren() {
  if (auto ast = BinopExp::downcast(this))
    return { ast->getLHS(), ast->getRHS() };
  if (auto ast = AscripExp::downcast(this))
    return { ast->getAscriptee(), ast->getAscripter() };
  if (auto ast = BlockExp::downcast(this))
    return { ast->getStatements() };
  if (auto ast = BorrowExp::downcast(this))
    return { ast->getRefExp() };
  if (auto ast = CallExp::downcast(this))
    return { ast->getFunction(), ast->getArguments() };
  if (auto ast = DataDecl::downcast(this))
    return { ast->getName(), ast->getFields() };
  if (auto ast = DeclList::downcast(this)) {
    llvm::SmallVector<AST*> ret;
    ret.reserve(ast->asArrayRef().size());
    for (auto elem : ast->asArrayRef()) ret.push_back(elem);
    return ret;
  }
  if (auto ast = DerefExp::downcast(this))
    return { ast->getOf() };
  if (auto ast = NameExp::downcast(this))
    return { ast->getName() };
  if (auto ast = ExpList::downcast(this)) {
    llvm::SmallVector<AST*> ret;
    for (auto elem : ast->asArrayRef()) ret.push_back(elem);
    return ret;
  }
  if (auto ast = FunctionDecl::downcast(this)) {
    if (ast->hasBody())
      return { ast->getName(), ast->getParameters(), ast->getReturnType(),
          ast->getBody() };
    else
      return { ast->getName(), ast->getParameters(), ast->getReturnType() };
  }
  if (auto ast = IfExp::downcast(this)) {
    if (ast->getElseExp() != nullptr)
      return { ast->getCondExp(), ast->getThenExp(), ast->getElseExp() };
    else
      return { ast->getCondExp(), ast->getThenExp() };
  }
  if (auto ast = IndexExp::downcast(this))
    return { ast->getBase(), ast->getIndex() };
  if (auto ast = LetExp::downcast(this)) {
    if (ast->getAscrip() != nullptr)
      return { ast->getBoundIdent(), ast->getAscrip(), ast->getDefinition() };
    else
      return { ast->getBoundIdent(), ast->getDefinition() };
  }
  if (auto ast = ModuleDecl::downcast(this))
    return { ast->getName(), ast->getDecls() };
  if (auto ast = MoveExp::downcast(this))
    return { ast->getRefExp() };
  if (auto ast = NameTypeExp::downcast(this))
    return { ast->getName() };
  if (auto ast = ParamList::downcast(this)) {
    llvm::SmallVector<AST*> ret;
    for (auto param : ast->asArrayRef()) {
      ret.push_back(param.first);
      ret.push_back(param.second);
    }
    return ret;
  }
  if (auto ast = ProjectExp::downcast(this))
    return { ast->getBase(), ast->getFieldName() };
  if (auto ast = RefExp::downcast(this))
    return { ast->getInitializer() };
  if (auto ast = RefTypeExp::downcast(this))
    return { ast->getPointeeType() };
  if (auto ast = ReturnExp::downcast(this))
    return { ast->getReturnee() };
  if (auto ast = StoreExp::downcast(this))
    return { ast->getLHS(), ast->getRHS() };
  if (auto ast = UnopExp::downcast(this))
    return { ast->getInner() };
  if (auto ast = WhileExp::downcast(this))
    return { ast->getCond(), ast->getBody() };
  return {};
}

const char* AST::IDToString(AST::ID id) {
  switch (id) {
  case AST::ID::ASCRIP:             return "ASCRIP";
  case AST::ID::BINOP_EXP:          return "BINOP_EXP";
  case AST::ID::BLOCK:              return "BLOCK";
  case AST::ID::BORROW:             return "BORROW";
  case AST::ID::BOOL_LIT:           return "BOOL_LIT";
  case AST::ID::CALL:               return "CALL";
  case AST::ID::DEC_LIT:            return "DEC_LIT";
  case AST::ID::DEREF:              return "DEREF";
  case AST::ID::ENAME:              return "ENAME";
  case AST::ID::IF:                 return "IF";
  case AST::ID::INDEX:              return "INDEX";
  case AST::ID::INT_LIT:            return "INT_LIT";
  case AST::ID::LET:                return "LET";
  case AST::ID::MOVE:               return "MOVE";
  case AST::ID::PROJECT:            return "PROJECT";
  case AST::ID::REF_EXP:            return "REF_EXP";
  case AST::ID::RETURN:             return "RETURN";
  case AST::ID::STORE:              return "STORE";
  case AST::ID::STRING_LIT:         return "STRING_LIT";
  case AST::ID::UNOP_EXP:           return "UNOP_EXP";
  case AST::ID::WHILE:              return "WHILE";

  case AST::ID::DATA:               return "DATA";
  case AST::ID::FUNC:               return "FUNC";
  case AST::ID::MODULE:             return "MODULE";

  case AST::ID::NAME_TEXP:          return "NAME_TEXP";
  case AST::ID::PRIMITIVE_TEXP:     return "PRIMITIVE_TEXP";
  case AST::ID::REF_TEXP:           return "REF_TEXP";
  
  case AST::ID::DECLLIST:           return "DECLLIST";
  case AST::ID::EXPLIST:            return "EXPLIST";
  case AST::ID::NAME:               return "NAME";
  case AST::ID::PARAMLIST:          return "PARAMLIST";
  }
  llvm_unreachable("AST::IDToString() unhandled switch case");
  return nullptr;
}

AST::ID stringToASTID(const std::string& str) {
       if (str == "ASCRIP")              return AST::ID::ASCRIP;
  else if (str == "BINOP_EXP")           return AST::ID::BINOP_EXP;
  else if (str == "BLOCK")               return AST::ID::BLOCK;
  else if (str == "BORROW")              return AST::ID::BORROW;
  else if (str == "BOOL_LIT")            return AST::ID::BOOL_LIT;
  else if (str == "CALL")                return AST::ID::CALL;
  else if (str == "DEC_LIT")             return AST::ID::DEC_LIT;
  else if (str == "DEREF")               return AST::ID::DEREF;
  else if (str == "ENAME")               return AST::ID::ENAME;
  else if (str == "IF")                  return AST::ID::IF;
  else if (str == "INDEX")               return AST::ID::INDEX;
  else if (str == "INT_LIT")             return AST::ID::INT_LIT;
  else if (str == "LET")                 return AST::ID::LET;
  else if (str == "MOVE")                return AST::ID::MOVE;
  else if (str == "PROJECT")             return AST::ID::PROJECT;
  else if (str == "REF_EXP")             return AST::ID::REF_EXP;
  else if (str == "RETURN")              return AST::ID::RETURN;
  else if (str == "STORE")               return AST::ID::STORE;
  else if (str == "STRING_LIT")          return AST::ID::STRING_LIT;
  else if (str == "UNOP_EXP")            return AST::ID::UNOP_EXP;
  else if (str == "WHILE")               return AST::ID::WHILE;

  else if (str == "DATA")                return AST::ID::DATA;
  else if (str == "FUNC")                return AST::ID::FUNC;
  else if (str == "MODULE")              return AST::ID::MODULE;
  
  else if (str == "NAME_TEXP")           return AST::ID::NAME_TEXP;
  else if (str == "PRIMITIVE_TEXP")      return AST::ID::PRIMITIVE_TEXP;
  else if (str == "REF_TEXP")            return AST::ID::REF_TEXP;
  
  else if (str == "DECLLIST")            return AST::ID::DECLLIST;
  else if (str == "EXPLIST")             return AST::ID::EXPLIST;
  else if (str == "NAME")                return AST::ID::NAME;
  else if (str == "PARAMLIST")           return AST::ID::PARAMLIST;

  else llvm_unreachable(("Invalid AST::ID string: " + str).c_str());
}

#endif