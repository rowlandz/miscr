#ifndef PARSER_PARSER
#define PARSER_PARSER

#include <cassert>
#include "common/Token.hpp"
#include "common/LocatedError.hpp"
#include "common/AST.hpp"

#define CHOMP_ELSE_ARREST(tokenTy, expected, element) if (p->ty == tokenTy) \
  ++p; else { errTryingToParse = element; expectedTokens = expected; \
  error = ARREST; return nullptr; }

#define EPSILON_ERROR { error = EPSILON; return nullptr; }

#define ARRESTING_ERROR { error = ARREST; return nullptr; }

/// If an error occurred, leave the error the same and return.
#define RETURN_IF_ERROR if (error != NOERROR) { return nullptr; }

/// If an error occurred, promote the error to an ARREST and return.
#define ARREST_IF_ERROR if (error != NOERROR){error = ARREST; return nullptr;}

/// @brief Return `r` following an arresting error or successful parse.
/// Otherwise reset `error` and continue this parse function. Used to implement
/// non-backtracking choice.
#define CONTINUE_ON_EPSILON(r) if (error != EPSILON) { return r; } else\
  { error = NOERROR; }



/// @brief The parser builds an AST on heap memory based on a vector of
/// `Token`s. Parser functions are named after the AST element that they parse
/// (e.g., `name`, `exp`, `decl`). If the parse was unsuccessful, `nullptr` is
/// returned and `getError` will construct a printable error message.
class Parser {
  const std::vector<Token>* tokens;
  std::vector<Token>::const_iterator p;
  std::vector<Token>::const_iterator end;

  /// Set when an error occurs.
  enum { NOERROR, EPSILON, ARREST } error = NOERROR;

  /// Set when an error occurs.
  const char* errTryingToParse = nullptr;

  /// Set when an error occurs.
  const char* expectedTokens = nullptr;

public:
  Parser(const std::vector<Token>* tokens) {
    this->tokens = tokens;
    p = tokens->begin();
    end = tokens->end();
  }

  /// Returns the parser error after an unsuccessful parse.
  LocatedError getError() {
    std::string errString;
    if (errTryingToParse != nullptr) {
      errString.append("I got stuck parsing ");
      errString.append(errTryingToParse);
      errString.append(".");
    } else {
      errString.append("I got stuck while parsing.");
    }
    if (expectedTokens != nullptr) {
      errString.append(" I was expecting ");
      errString.append(expectedTokens);
      errString.append(" next.");
    }
    return LocatedError(tokToLoc(p), errString);
  }

  // ---------- Idents and QIdents ----------

  /// Parses an identifier (i.e., an unqualified name). 
  Name* ident() {
    if (p->ty == TOK_IDENT) return new Name(tokToLoc(p), (p++)->asString());
    EPSILON_ERROR
  }

  /// Same as `ident` except the result is returned as an `std::string`.
  std::string namePart() {
    if (p->ty == TOK_IDENT) return (p++)->asString();
    error = EPSILON;
    return std::string();
  }

  /// Parses a name (e.g., `MyModule::AnotherModule::myfunc`).
  Name* name() {
    Token t = *p;
    std::string s = namePart(); RETURN_IF_ERROR
    while (p->ty == COLON_COLON) {
      ++p;
      std::string s2 = namePart(); ARREST_IF_ERROR
      s.append("::");
      s.append(s2);
    }
    Location loc(t.row, t.col, (unsigned int)(p->ptr - t.ptr));
    return new Name(loc, s);
  }

  // ---------- Expressions ----------

  BoolLit* boolLit() {
    if (p->ty == KW_FALSE) return new BoolLit(tokToLoc(p++), false);
    if (p->ty == KW_TRUE) return new BoolLit(tokToLoc(p++), true);
    EPSILON_ERROR
  }

  IntLit* intLit() {
    if (p->ty == LIT_INT) return new IntLit(tokToLoc(p), (p++)->ptr);
    EPSILON_ERROR
  }

  DecimalLit* decimalLit() {
    if (p->ty == LIT_DEC) return new DecimalLit(tokToLoc(p), (p++)->ptr);
    EPSILON_ERROR
  }

  StringLit* stringLit() {
    if (p->ty == LIT_STRING) return new StringLit(tokToLoc(p), (p++)->ptr);
    EPSILON_ERROR
  }

  Exp* parensExp() {
    if (p->ty == LPAREN) ++p; else EPSILON_ERROR
    Exp* ret = exp(); ARREST_IF_ERROR
    CHOMP_ELSE_ARREST(RPAREN, ")", "parentheses expression")
    return ret;
  }

  Exp* nameOrCallExp() {
    Token t = *p;
    Name* n = name(); RETURN_IF_ERROR
    if (p->ty == LPAREN) {
      ++p;
      ExpList* argList = expListWotc0();
      CHOMP_ELSE_ARREST(RPAREN, ")", "function call")
      Location loc(t.row, t.col, (unsigned int)(p->ptr - t.ptr));
      return new CallExp(loc, n, argList);
    } else {
      return new NameExp(n->getLocation(), n);  // TODO: reduce to only one argument
    }
  }

  BlockExp* blockExp() {
    Token t = *p;
    if (p->ty == LBRACE) ++p; else EPSILON_ERROR
    ExpList* statements = stmts0(); ARREST_IF_ERROR
    CHOMP_ELSE_ARREST(RBRACE, "}", "block expression")
    Location loc(t.row, t.col, (unsigned int)(p->ptr - t.ptr));
    return new BlockExp(loc, statements);
  }

  /// @brief Parses an array list or array init expression. 
  Exp* arrayListOrInitExp() {
    Token t0 = *p;
    if (p->ty == LBRACKET) ++p; else EPSILON_ERROR
    Token t1 = *p;
    Exp* firstExp = exp(); ARREST_IF_ERROR
    if (p->ty == KW_OF) {
      ++p;
      Exp* initializer = exp(); ARREST_IF_ERROR
      CHOMP_ELSE_ARREST(RBRACKET, "]", "array init expression")
      Location loc(t0.row, t0.col, (unsigned int)(p->ptr - t0.ptr));
      return new ArrayInitExp(loc, firstExp, initializer);
    } else if (p->ty == COMMA) {
      ++p;
      ExpList* contentTail = expListWotc0(); ARREST_IF_ERROR
      Location loc(t1.row, t1.col, (unsigned int)(p->ptr - t1.ptr));
      ExpList* content = new ExpList(loc, firstExp, contentTail);
      CHOMP_ELSE_ARREST(RBRACKET, "]", "array list constructor")
      loc = Location(t0.row, t0.col, (unsigned int)(p->ptr - t0.ptr));
      return new ArrayListExp(loc, content);
    } else if (p->ty == RBRACKET) {
      ExpList* content = new ExpList(Location(p->row, p->col, 0));
      ++p;
      Location loc(t0.row, t0.col, (unsigned int)(p->ptr - t0.ptr));
      return new ArrayListExp(loc, content);
    } else {
      errTryingToParse = "array init or list expression";
      expectedTokens = "of , ]";
      ARRESTING_ERROR
    }
  }

  Exp* expLv0() {
    Exp* ret;
    ret = nameOrCallExp(); CONTINUE_ON_EPSILON(ret)
    ret = boolLit(); CONTINUE_ON_EPSILON(ret)
    ret = intLit(); CONTINUE_ON_EPSILON(ret)
    ret = decimalLit(); CONTINUE_ON_EPSILON(ret)
    ret = stringLit(); CONTINUE_ON_EPSILON(ret)
    ret = parensExp(); CONTINUE_ON_EPSILON(ret)
    ret = blockExp(); CONTINUE_ON_EPSILON(ret)
    ret = arrayListOrInitExp(); CONTINUE_ON_EPSILON(ret)
    EPSILON_ERROR
  }

  Exp* expLv1() {
    Token t = *p;
    Exp* e = expLv0(); RETURN_IF_ERROR
    for (;;) {
      switch(p->ty) {
      case EXCLAIM: {
        Location loc(t.row, t.col, (unsigned int)(p->ptr - t.ptr));
        e = new DerefExp(loc, e);
        ++p;
        break;
      }
      case LBRACKET: {
        ++p;
        Exp* indexExp = exp(); ARREST_IF_ERROR
        CHOMP_ELSE_ARREST(RBRACKET, "]", "index expression")
        Location loc(t.row, t.col, (unsigned int)(p->ptr - t.ptr));
        e = new IndexExp(loc, e, indexExp);
        break;
      }
      default: return e;
      }
    }
  }

  Exp* expLv2() {
    Token t = *p;
    if (t.ty == AMP || t.ty == HASH) {
      ++p;
      Exp* e = expLv2(); ARREST_IF_ERROR
      Location loc(t.row, t.col, (unsigned int)(p->ptr - t.ptr));
      return new RefExp(loc, e, t.ty == HASH);
    } else {
      return expLv1();
    }
  }

  Exp* expLv3() {
    Token t = *p;
    Exp* e = expLv2(); RETURN_IF_ERROR
    while (p->ty == COLON) {
      ++p;
      TypeExp* tyAscrip = typeExp(); ARREST_IF_ERROR
      Location loc(t.row, t.col, (unsigned int)(p->ptr - t.ptr));
      e = new AscripExp(loc, e, tyAscrip);
    }
    return e;
  }

  Exp* expLv4() {
    Token t = *p;
    Exp* lhs = expLv3();
    RETURN_IF_ERROR
    while (p->ty == OP_MUL || p->ty == OP_DIV) {
      AST::ID op = (p->ty == OP_MUL) ? AST::ID::MUL : AST::ID::DIV;
      ++p;
      Exp* rhs = expLv3(); ARREST_IF_ERROR
      Location loc(t.row, t.col, (unsigned int)(p->ptr - t.ptr));
      lhs = new BinopExp(op, loc, lhs, rhs);
    }
    return lhs;
  }

  Exp* expLv5() {
    Token t = *p;
    Exp* lhs = expLv4(); RETURN_IF_ERROR
    while (p->ty == OP_ADD || p->ty == OP_SUB) {
      AST::ID op = (p->ty == OP_ADD) ? AST::ID::ADD : AST::ID::SUB;
      ++p;
      Exp* rhs = expLv4(); ARREST_IF_ERROR
      Location loc(t.row, t.col, (unsigned int)(p->ptr - t.ptr));
      lhs = new BinopExp(op, loc, lhs, rhs);
    }
    return lhs;
  }

  Exp* expLv6() {
    Token t = *p;
    Exp* lhs = expLv5(); RETURN_IF_ERROR
    while (p->ty == OP_EQ || p->ty == OP_NEQ || p->ty == OP_GE
    || p->ty == OP_GT || p->ty == OP_LE || p->ty == OP_LT) {
      AST::ID op = p->ty == OP_EQ ? AST::ID::EQ :
                   p->ty == OP_NEQ ? AST::ID::NE :
                   p->ty == OP_GE ? AST::ID::GE :
                   p->ty == OP_GT ? AST::ID::GT :
                   p->ty == OP_LE ? AST::ID::LE :
                                    AST::ID::LT;
      ++p;
      Exp* rhs = expLv5(); ARREST_IF_ERROR
      Location loc(t.row, t.col, (unsigned int)(p->ptr - t.ptr));
      lhs = new BinopExp(op, loc, lhs, rhs);
    }
    return lhs;
  }

  Exp* expLv7() {
    Token t = *p;
    Exp* lhs = expLv6(); RETURN_IF_ERROR
    while (p->ty == COLON_EQUAL) {
      ++p;
      Exp* rhs = expLv6(); ARREST_IF_ERROR
      Location loc(t.row, t.col, (unsigned int)(p->ptr - t.ptr));
      lhs = new StoreExp(loc, lhs, rhs);
    }
    return lhs;
  }

  IfExp* ifExp() {
    Token t = *p;
    if (p->ty == KW_IF) ++p; else EPSILON_ERROR
    Exp* condExp = exp(); ARREST_IF_ERROR
    CHOMP_ELSE_ARREST(KW_THEN, "then", "if expression")
    Exp* thenExp = exp(); ARREST_IF_ERROR
    CHOMP_ELSE_ARREST(KW_ELSE, "else", "if expression")
    Exp* elseExp = exp(); ARREST_IF_ERROR
    Location loc(t.row, t.col, (unsigned int)(p->ptr - t.ptr));
    return new IfExp(loc, condExp, thenExp, elseExp);
  }

  Exp* exp() {
    Exp* ret = ifExp(); CONTINUE_ON_EPSILON(ret)
    return expLv7();
  }

  /// Zero or more comma-separated expressions with optional trailing comma
  /// (wotc). Never epsilon fails.
  ExpList* expListWotc0() {
    Token t = *p;
    llvm::SmallVector<Exp*, 4> es;
    Exp* e;
    for (;;) {
      e = exp();
      if (error == ARREST) ARRESTING_ERROR
      if (error == EPSILON) {
        error = NOERROR;
        Location loc(t.row, t.col, (unsigned int)(p->ptr - t.ptr));
        return new ExpList(loc, es);
      }
      es.push_back(e);
      if (p->ty != COMMA) {
        Location loc(t.row, t.col, (unsigned int)(p->ptr - t.ptr));
        return new ExpList(loc, es);
      }
      ++p;
    }
  }

  // ---------- Type Expressions ----------

  TypeExp* typeExpLv0() {
    if (p->ty == KW_f32) return TypeExp::newF32(tokToLoc(p++));
    if (p->ty == KW_f64) return TypeExp::newF64(tokToLoc(p++));
    if (p->ty == KW_i8) return TypeExp::newI8(tokToLoc(p++));
    if (p->ty == KW_i16) return TypeExp::newI16(tokToLoc(p++));
    if (p->ty == KW_i32) return TypeExp::newI32(tokToLoc(p++));
    if (p->ty == KW_i64) return TypeExp::newI64(tokToLoc(p++));
    if (p->ty == KW_BOOL) return TypeExp::newBool(tokToLoc(p++));
    if (p->ty == KW_STR) return TypeExp::newStr(tokToLoc(p++));
    if (p->ty == KW_UNIT) return TypeExp::newUnit(tokToLoc(p++));
    TypeExp* ret = arrayTypeExp(); CONTINUE_ON_EPSILON(ret)
    EPSILON_ERROR
  }

  TypeExp* typeExp() {
    Token t = *p;
    if (t.ty == AMP || t.ty == HASH) {
      ++p;
      TypeExp* n1 = typeExp(); ARREST_IF_ERROR
      Location loc(t.row, t.col, (unsigned int)(p->ptr - t.ptr));
      return new RefTypeExp(loc, n1, t.ty == HASH);
    } else {
      return typeExpLv0();
    }
  }

  ArrayTypeExp* arrayTypeExp() {
    Token t = *p;
    if (p->ty == LBRACKET) ++p; else EPSILON_ERROR
    Exp* size = nullptr;
    if (p->ty == UNDERSCORE) {
      ++p;
    } else {
      size = exp(); ARREST_IF_ERROR
    }
    CHOMP_ELSE_ARREST(KW_OF, "of", "array type expression")
    TypeExp* innerType = typeExp(); ARREST_IF_ERROR
    CHOMP_ELSE_ARREST(RBRACKET, "]", "array type expression")
    Location loc(t.row, t.col, (unsigned int)(p->ptr - t.ptr));
    return new ArrayTypeExp(loc, size, innerType);
  }

  // ---------- Statements ----------

  LetExp* letStmt() {
    Token t = *p;
    if (p->ty == KW_LET) ++p; else EPSILON_ERROR
    Name* boundIdent = ident(); ARREST_IF_ERROR
    TypeExp* ascrip = nullptr;
    if (p->ty == COLON) {
      ++p;
      ascrip = typeExp(); ARREST_IF_ERROR
      CHOMP_ELSE_ARREST(EQUAL, "=", "let statement")
    } else {
      CHOMP_ELSE_ARREST(EQUAL, ": =", "let statement")
    }
    Exp* definition = exp(); ARREST_IF_ERROR
    Location loc(t.row, t.col, (unsigned int)(p->ptr - t.ptr));
    return new LetExp(loc, boundIdent, ascrip, definition);
  }

  ReturnExp* returnStmt() {
    Token t = *p;
    if (p->ty == KW_RETURN) ++p; else EPSILON_ERROR
    Exp* returnee = exp(); ARREST_IF_ERROR
    CHOMP_ELSE_ARREST(SEMICOLON, ";", "return statement")
    Location loc(t.row, t.col, (unsigned int)(p->ptr - t.ptr));
    return new ReturnExp(loc, returnee);
  }

  Exp* stmt() {
    Exp* ret;
    ret = letStmt(); CONTINUE_ON_EPSILON(ret)
    ret = returnStmt(); CONTINUE_ON_EPSILON(ret)
    ret = exp(); CONTINUE_ON_EPSILON(ret)
    EPSILON_ERROR
  }

  /// Zero or more statements separated by semicolons Never epsilon fails.
  ExpList* stmts0() {
    Token t = *p;
    llvm::SmallVector<Exp*, 4> ss;
    Exp* s;
    for (;;) {
      s = stmt();
      if (error == ARREST) ARRESTING_ERROR
      if (error == EPSILON) {
        error = NOERROR;
        Location loc(t.row, t.col, (unsigned int)(p->ptr - t.ptr));
        return new ExpList(loc, ss);
      }
      ss.push_back(s);
      if (p->ty != SEMICOLON) {
        Location loc(t.row, t.col, (unsigned int)(p->ptr - t.ptr));
        return new ExpList(loc, ss);
      }
      ++p;
    }
  }

  // ---------- Declarations ----------

  /// Parses parameter list of length zero or higher with optional terminal
  /// comma (wotc). This parser never returns an epsilon error.
  ParamList* paramListWotc0() {
    Token t = *p;
    llvm::SmallVector<std::pair<Name*, TypeExp*>, 4> params;
    Name* paramName;
    TypeExp* paramType;
    for (;;) {
      paramName = ident();
      if (error == ARREST) return nullptr;
      if (error == EPSILON) {
        error = NOERROR;
        Location loc(t.row, t.col, (unsigned int)(p->ptr - t.ptr));
        return new ParamList(loc, params);
      }
      CHOMP_ELSE_ARREST(COLON, ":", "parameter list")
      paramType = typeExp(); ARREST_IF_ERROR
      params.push_back(std::pair<Name*, TypeExp*>(paramName, paramType));
      if (p->ty != COMMA) {
        Location loc(t.row, t.col, (unsigned int)(p->ptr - t.ptr));
        return new ParamList(loc, params);
      }
      ++p;
    }
  }

  DataDecl* dataDecl() {
    Token t = *p;
    if (p->ty == KW_DATA) ++p; else EPSILON_ERROR
    Name* name = ident(); ARREST_IF_ERROR
    CHOMP_ELSE_ARREST(LPAREN, "(", "data")
    ParamList* fields = paramListWotc0(); ARREST_IF_ERROR
    CHOMP_ELSE_ARREST(RPAREN, ")", "data")
    Location loc(t.row, t.col, (unsigned int)(p->ptr - t.ptr));
    return new DataDecl(loc, name, fields);
  }

  FunctionDecl* functionDecl() {
    Token t = *p;
    if (p->ty == KW_FUNC) ++p; else EPSILON_ERROR
    Name* name = ident(); ARREST_IF_ERROR
    CHOMP_ELSE_ARREST(LPAREN, "(", "function")
    ParamList* params = paramListWotc0(); ARREST_IF_ERROR
    CHOMP_ELSE_ARREST(RPAREN, ")", "function")
    CHOMP_ELSE_ARREST(COLON, ":", "function")
    TypeExp* retType = typeExp(); ARREST_IF_ERROR
    CHOMP_ELSE_ARREST(EQUAL, "=", "function")
    Exp* body = exp(); ARREST_IF_ERROR
    CHOMP_ELSE_ARREST(SEMICOLON, ";", "function")
    Location loc(t.row, t.col, (unsigned int)(p->ptr - t.ptr));
    return new FunctionDecl(loc, name, params, retType, body);
  }

  FunctionDecl* externFunctionDecl() {
    Token t = *p;
    if (p->ty == KW_EXTERN) ++p; else EPSILON_ERROR
    CHOMP_ELSE_ARREST(KW_FUNC, "func", "extern function")
    Name* name = ident(); ARREST_IF_ERROR
    CHOMP_ELSE_ARREST(LPAREN, "(", "extern function")
    ParamList* params = paramListWotc0(); ARREST_IF_ERROR
    CHOMP_ELSE_ARREST(RPAREN, ")", "extern function")
    CHOMP_ELSE_ARREST(COLON, ":", "extern function")
    TypeExp* retType = typeExp(); ARREST_IF_ERROR
    CHOMP_ELSE_ARREST(SEMICOLON, ";", "extern function")
    Location loc(t.row, t.col, (unsigned int)(p->ptr - t.ptr));
    return new FunctionDecl(loc, name, params, retType);
  }

  /** Parses a module or namespace with brace-syntax. */
  Decl* moduleOrNamespace() {
    Token t = *p;
    AST::ID ty;
    if (p->ty == KW_MODULE) { ty = AST::ID::MODULE; ++p; }
    else if (p->ty == KW_NAMESPACE) { ty = AST::ID::NAMESPACE; ++p; }
    else EPSILON_ERROR
    const char* whatThisIs = ty == AST::ID::MODULE ? "module" : "namespace";
    Name* n1 = ident(); ARREST_IF_ERROR
    CHOMP_ELSE_ARREST(LBRACE, "{", whatThisIs)
    DeclList* n2 = decls0(); ARREST_IF_ERROR
    CHOMP_ELSE_ARREST(RBRACE, "}", whatThisIs)
    Location loc(t.row, t.col, (unsigned int)(p->ptr - t.ptr));
    return ty == AST::ID::MODULE ?
           static_cast<Decl*>(new ModuleDecl(loc, n1, n2)) :
           new NamespaceDecl(loc, n1, n2);
  }

  /** A declaration is a func, proc, data type, module, or namespace. */
  Decl* decl() {
    Decl* ret;
    ret = dataDecl(); CONTINUE_ON_EPSILON(ret)
    ret = functionDecl(); CONTINUE_ON_EPSILON(ret)
    ret = externFunctionDecl(); CONTINUE_ON_EPSILON(ret)
    ret = moduleOrNamespace(); CONTINUE_ON_EPSILON(ret)
    EPSILON_ERROR
  }

  /// Parses zero or more declarations into a DECLLIST.
  DeclList* decls0() {
    Token t = *p;
    llvm::SmallVector<Decl*, 0> ds;
    Decl* d;
    for (;;) {
      d = decl();
      if (error == ARREST) ARRESTING_ERROR
      if (error == EPSILON) {
        error = NOERROR;
        Location loc(t.row, t.col, (unsigned int)(p->ptr - t.ptr));
        return new DeclList(loc, ds);
      }
      ds.push_back(d);
    }
  }

  // ----------

  Location tokToLoc(std::vector<Token>::const_iterator tok) {
    return Location(tok->row, tok->col, tok->sz);
  }
};

#endif