#ifndef PARSER_PARSER
#define PARSER_PARSER

#include <cassert>
#include "common/Token.hpp"
#include "common/LocatedError.hpp"
#include "common/AST.hpp"

#define CHOMP_ELSE_ARREST(tokenTy, expected, element) if (p->tag == tokenTy) \
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
  const std::vector<Token>& tokens;
  std::vector<Token>::const_iterator p;
  std::vector<Token>::const_iterator end;

  /// Set when an error occurs.
  enum { NOERROR, EPSILON, ARREST } error = NOERROR;

  /// Set when an error occurs.
  const char* errTryingToParse = nullptr;

  /// Set when an error occurs.
  const char* expectedTokens = nullptr;

public:
  Parser(const std::vector<Token>& tokens)
    : tokens(tokens), p(tokens.begin()), end(tokens.end()) {}

  /// @brief Returns the parser error after an unsuccessful parse.
  LocatedError getError() {
    LocatedError err('^');
    if (errTryingToParse != nullptr) {
      err.append("I got stuck parsing ");
      err.append(errTryingToParse);
      err.append(".");
    } else {
      err.append("I got stuck while parsing.\n");
    }
    if (expectedTokens != nullptr) {
      err.append(" I was expecting ");
      err.append(expectedTokens);
      err.append(" next.\n");
    }
    err.append(p->loc);
    return err;
  }

  // ---------- Idents and QIdents ----------

  /// @brief Parses an identifier (i.e., an unqualified name). 
  Name* ident() {
    if (p->tag == Token::TOK_IDENT) return new Name(p->loc, (p++)->asString());
    EPSILON_ERROR
  }

  /// @brief Same as ident() except the result is returned as an `std::string`.
  std::string namePart() {
    if (p->tag == Token::TOK_IDENT) return (p++)->asString();
    error = EPSILON;
    return std::string();
  }

  /// @brief Parses a name (e.g., `MyModule::AnotherModule::myfunc`).
  Name* name() {
    Token t = *p;
    std::string s = namePart(); RETURN_IF_ERROR
    while (p->tag == Token::COLON_COLON) {
      ++p;
      std::string s2 = namePart(); ARREST_IF_ERROR
      s.append("::");
      s.append(s2);
    }
    return new Name(hereFrom(t), s);
  }

  // ---------- Expressions ----------

  BoolLit* boolLit() {
    if (p->tag == Token::KW_FALSE) return new BoolLit((p++)->loc, false);
    if (p->tag == Token::KW_TRUE) return new BoolLit((p++)->loc, true);
    EPSILON_ERROR
  }

  IntLit* intLit() {
    if (p->tag == Token::LIT_INT) return new IntLit(p->loc, (p++)->ptr);
    EPSILON_ERROR
  }

  DecimalLit* decimalLit() {
    if (p->tag == Token::LIT_DEC) return new DecimalLit(p->loc, (p++)->ptr);
    EPSILON_ERROR
  }

  StringLit* stringLit() {
    if (p->tag == Token::LIT_STRING) return new StringLit(p->loc, (p++)->ptr);
    EPSILON_ERROR
  }

  Exp* parensExp() {
    if (p->tag == Token::LPAREN) ++p; else EPSILON_ERROR
    Exp* ret = exp(); ARREST_IF_ERROR
    CHOMP_ELSE_ARREST(Token::RPAREN, ")", "parentheses expression")
    return ret;
  }

  Exp* nameOrCallExp() {
    Token t = *p;
    Name* n = name(); RETURN_IF_ERROR
    if (p->tag == Token::LPAREN) {
      ++p;
      ExpList* argList = expListWotc0();
      CHOMP_ELSE_ARREST(Token::RPAREN, ")", "function call")
      return new CallExp(hereFrom(t), n, argList);
    } else {
      return new NameExp(n->getLocation(), n);  // TODO: reduce to only one argument
    }
  }

  BlockExp* blockExp() {
    Token t = *p;
    if (p->tag == Token::LBRACE) ++p; else EPSILON_ERROR
    ExpList* statements = stmts0(); ARREST_IF_ERROR
    CHOMP_ELSE_ARREST(Token::RBRACE, "}", "block expression")
    return new BlockExp(hereFrom(t), statements);
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
    EPSILON_ERROR
  }

  Exp* expLv1() {
    Token t = *p;
    Exp* e = expLv0(); RETURN_IF_ERROR
    for (;;) {
      switch(p->tag) {
      case Token::EXCLAIM: {
        ++p;
        e = new DerefExp(hereFrom(t), e);
        break;
      }
      case Token::LBRACKET: {
        ++p;
        if (p->tag == Token::DOT) {
          ++p;
          Name* fieldName = ident(); ARREST_IF_ERROR
          CHOMP_ELSE_ARREST(Token::RBRACKET, "]", "index field expression")
          e = new IndexFieldExp(hereFrom(t), e, fieldName);
          break;
        }
        else {
          Exp* indexExp = exp(); ARREST_IF_ERROR
          CHOMP_ELSE_ARREST(Token::RBRACKET, "]", "index expression")
          e = new IndexExp(hereFrom(t), e, indexExp);
          break;
        }
      }
      default: return e;
      }
    }
  }

  Exp* expLv2() {
    Token t = *p;
    if (t.tag == Token::AMP) {
      ++p;
      Exp* e = expLv2(); ARREST_IF_ERROR
      return new RefExp(hereFrom(t), e);
    } else {
      return expLv1();
    }
  }

  Exp* expLv25() {
    Token t = *p;
    if (t.tag == Token::KW_BORROW) {
      ++p;
      Exp* e = expLv25(); ARREST_IF_ERROR
      return new BorrowExp(hereFrom(t), e);
    } else if (t.tag == Token::KW_MOVE) {
      ++p;
      Exp* e = expLv25(); ARREST_IF_ERROR
      return new MoveExp(hereFrom(t), e);
    } else {
      return expLv2();
    }
  }

  Exp* expLv3() {
    Token t = *p;
    Exp* e = expLv25(); RETURN_IF_ERROR
    while (p->tag == Token::COLON) {
      ++p;
      TypeExp* tyAscrip = typeExp(); ARREST_IF_ERROR
      e = new AscripExp(hereFrom(t), e, tyAscrip);
    }
    return e;
  }

  Exp* expLv4() {
    Token t = *p;
    Exp* lhs = expLv3();
    RETURN_IF_ERROR
    while (p->tag == Token::OP_MUL || p->tag == Token::OP_DIV) {
      AST::ID op = (p->tag == Token::OP_MUL) ? AST::ID::MUL : AST::ID::DIV;
      ++p;
      Exp* rhs = expLv3(); ARREST_IF_ERROR
      lhs = new BinopExp(op, hereFrom(t), lhs, rhs);
    }
    return lhs;
  }

  Exp* expLv5() {
    Token t = *p;
    Exp* lhs = expLv4(); RETURN_IF_ERROR
    while (p->tag == Token::OP_ADD || p->tag == Token::OP_SUB) {
      AST::ID op = (p->tag == Token::OP_ADD) ? AST::ID::ADD : AST::ID::SUB;
      ++p;
      Exp* rhs = expLv4(); ARREST_IF_ERROR
      lhs = new BinopExp(op, hereFrom(t), lhs, rhs);
    }
    return lhs;
  }

  Exp* expLv6() {
    Token t = *p;
    Exp* lhs = expLv5(); RETURN_IF_ERROR
    while (p->tag == Token::OP_EQ || p->tag == Token::OP_NE || p->tag == Token::OP_GE
    || p->tag == Token::OP_GT || p->tag == Token::OP_LE || p->tag == Token::OP_LT) {
      AST::ID op = p->tag == Token::OP_EQ ? AST::ID::EQ :
                   p->tag == Token::OP_NE ? AST::ID::NE :
                   p->tag == Token::OP_GE ? AST::ID::GE :
                   p->tag == Token::OP_GT ? AST::ID::GT :
                   p->tag == Token::OP_LE ? AST::ID::LE :
                                            AST::ID::LT;
      ++p;
      Exp* rhs = expLv5(); ARREST_IF_ERROR
      lhs = new BinopExp(op, hereFrom(t), lhs, rhs);
    }
    return lhs;
  }

  Exp* expLv7() {
    Token t = *p;
    Exp* lhs = expLv6(); RETURN_IF_ERROR
    while (p->tag == Token::COLON_EQUAL) {
      ++p;
      Exp* rhs = expLv6(); ARREST_IF_ERROR
      lhs = new StoreExp(hereFrom(t), lhs, rhs);
    }
    return lhs;
  }

  IfExp* ifExp() {
    Token t = *p;
    if (p->tag == Token::KW_IF) ++p; else EPSILON_ERROR
    Exp* condExp = exp(); ARREST_IF_ERROR
    CHOMP_ELSE_ARREST(Token::KW_THEN, "then", "if expression")
    Exp* thenExp = exp(); ARREST_IF_ERROR
    CHOMP_ELSE_ARREST(Token::KW_ELSE, "else", "if expression")
    Exp* elseExp = exp(); ARREST_IF_ERROR
    return new IfExp(hereFrom(t), condExp, thenExp, elseExp);
  }

  Exp* exp() {
    Exp* ret = ifExp(); CONTINUE_ON_EPSILON(ret)
    return expLv7();
  }

  /// @brief Zero or more comma-separated expressions with optional trailing
  /// comma (wotc). Never epsilon fails.
  ExpList* expListWotc0() {
    Token t = *p;
    llvm::SmallVector<Exp*, 4> es;
    Exp* e;
    for (;;) {
      e = exp();
      if (error == ARREST) ARRESTING_ERROR
      if (error == EPSILON) {
        error = NOERROR;
        return new ExpList(hereFrom(t), es);
      }
      es.push_back(e);
      if (p->tag != Token::COMMA) {
        return new ExpList(hereFrom(t), es);
      }
      ++p;
    }
  }

  // ---------- Type Expressions ----------

  TypeExp* typeExpLv0() {
    if (p->tag == Token::KW_f32) return PrimitiveTypeExp::newF32((p++)->loc);
    if (p->tag == Token::KW_f64) return PrimitiveTypeExp::newF64((p++)->loc);
    if (p->tag == Token::KW_i8) return PrimitiveTypeExp::newI8((p++)->loc);
    if (p->tag == Token::KW_i16) return PrimitiveTypeExp::newI16((p++)->loc);
    if (p->tag == Token::KW_i32) return PrimitiveTypeExp::newI32((p++)->loc);
    if (p->tag == Token::KW_i64) return PrimitiveTypeExp::newI64((p++)->loc);
    if (p->tag == Token::KW_BOOL) return PrimitiveTypeExp::newBool((p++)->loc);
    if (p->tag == Token::KW_UNIT) return PrimitiveTypeExp::newUnit((p++)->loc);
    TypeExp* ret;
    ret = nameTypeExp(); CONTINUE_ON_EPSILON(ret)
    EPSILON_ERROR
  }

  TypeExp* typeExp() {
    Token t = *p;
    if (t.tag == Token::AMP || t.tag == Token::HASH) {
      bool owned = t.tag == Token::HASH;
      ++p;
      TypeExp* n1 = typeExp(); ARREST_IF_ERROR
      return new RefTypeExp(hereFrom(t), n1, owned);
    } else {
      return typeExpLv0();
    }
  }

  NameTypeExp* nameTypeExp() {
    Name* n = name(); RETURN_IF_ERROR
    return new NameTypeExp(n);
  }

  // ---------- Statements ----------

  LetExp* letStmt() {
    Token t = *p;
    if (p->tag == Token::KW_LET) ++p; else EPSILON_ERROR
    Name* boundIdent = ident(); ARREST_IF_ERROR
    TypeExp* ascrip = nullptr;
    if (p->tag == Token::COLON) {
      ++p;
      ascrip = typeExp(); ARREST_IF_ERROR
      CHOMP_ELSE_ARREST(Token::EQUAL, "=", "let statement")
    } else {
      CHOMP_ELSE_ARREST(Token::EQUAL, ": =", "let statement")
    }
    Exp* definition = exp(); ARREST_IF_ERROR
    return new LetExp(hereFrom(t), boundIdent, ascrip, definition);
  }

  ReturnExp* returnStmt() {
    Token t = *p;
    if (p->tag == Token::KW_RETURN) ++p; else EPSILON_ERROR
    Exp* returnee = exp(); ARREST_IF_ERROR
    CHOMP_ELSE_ARREST(Token::SEMICOLON, ";", "return statement")
    return new ReturnExp(hereFrom(t), returnee);
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
        return new ExpList(hereFrom(t), ss);
      }
      ss.push_back(s);
      if (p->tag != Token::SEMICOLON) {
        return new ExpList(hereFrom(t), ss);
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
        return new ParamList(hereFrom(t), params);
      }
      CHOMP_ELSE_ARREST(Token::COLON, ":", "parameter list")
      paramType = typeExp(); ARREST_IF_ERROR
      params.push_back(std::pair<Name*, TypeExp*>(paramName, paramType));
      if (p->tag != Token::COMMA) {
        return new ParamList(hereFrom(t), params);
      }
      ++p;
    }
  }

  DataDecl* dataDecl() {
    Token t = *p;
    if (p->tag == Token::KW_DATA) ++p; else EPSILON_ERROR
    Name* name = ident(); ARREST_IF_ERROR
    CHOMP_ELSE_ARREST(Token::LPAREN, "(", "data")
    ParamList* fields = paramListWotc0(); ARREST_IF_ERROR
    CHOMP_ELSE_ARREST(Token::RPAREN, ")", "data")
    return new DataDecl(hereFrom(t), name, fields);
  }

  FunctionDecl* functionDecl() {
    Token t = *p;
    if (p->tag == Token::KW_FUNC) ++p; else EPSILON_ERROR
    Name* name = ident(); ARREST_IF_ERROR
    CHOMP_ELSE_ARREST(Token::LPAREN, "(", "function")
    ParamList* params = paramListWotc0(); ARREST_IF_ERROR
    CHOMP_ELSE_ARREST(Token::RPAREN, ")", "function")
    CHOMP_ELSE_ARREST(Token::COLON, ":", "function")
    TypeExp* retType = typeExp(); ARREST_IF_ERROR
    CHOMP_ELSE_ARREST(Token::EQUAL, "=", "function")
    Exp* body = exp(); ARREST_IF_ERROR
    CHOMP_ELSE_ARREST(Token::SEMICOLON, ";", "function")
    return new FunctionDecl(hereFrom(t), name, params, retType, body);
  }

  FunctionDecl* externFunctionDecl() {
    Token t = *p;
    if (p->tag == Token::KW_EXTERN) ++p; else EPSILON_ERROR
    CHOMP_ELSE_ARREST(Token::KW_FUNC, "func", "extern function")
    Name* name = ident(); ARREST_IF_ERROR
    CHOMP_ELSE_ARREST(Token::LPAREN, "(", "extern function")
    ParamList* params = paramListWotc0(); ARREST_IF_ERROR
    CHOMP_ELSE_ARREST(Token::RPAREN, ")", "extern function")
    CHOMP_ELSE_ARREST(Token::COLON, ":", "extern function")
    TypeExp* retType = typeExp(); ARREST_IF_ERROR
    CHOMP_ELSE_ARREST(Token::SEMICOLON, ";", "extern function")
    return new FunctionDecl(hereFrom(t), name, params, retType);
  }

  /// @brief Parses a module or namespace with brace-syntax.
  Decl* moduleOrNamespace() {
    Token t = *p;
    AST::ID ty;
    if (p->tag == Token::KW_MODULE) { ty = AST::ID::MODULE; ++p; }
    else if (p->tag == Token::KW_NAMESPACE) { ty = AST::ID::NAMESPACE; ++p; }
    else EPSILON_ERROR
    const char* whatThisIs = ty == AST::ID::MODULE ? "module" : "namespace";
    Name* n1 = ident(); ARREST_IF_ERROR
    CHOMP_ELSE_ARREST(Token::LBRACE, "{", whatThisIs)
    DeclList* n2 = decls0(); ARREST_IF_ERROR
    CHOMP_ELSE_ARREST(Token::RBRACE, "}", whatThisIs)
    return ty == AST::ID::MODULE ?
           static_cast<Decl*>(new ModuleDecl(hereFrom(t), n1, n2)) :
           new NamespaceDecl(hereFrom(t), n1, n2);
  }

  /// @brief A declaration is a func, proc, data type, module, or namespace.
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
        return new DeclList(hereFrom(t), ds);
      }
      ds.push_back(d);
    }
  }

  // ----------

  /// @brief Returns the location that spans from the beginning of @p first to
  /// the end of the token before the current one. Make sure there _is_ a
  /// previous token before calling this function.
  Location hereFrom(Token first) {
    return Location(first.loc.row, first.loc.col,
      (p-1)->ptr - first.ptr + (p-1)->loc.sz);
  }
};

#endif