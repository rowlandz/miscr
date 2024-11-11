#ifndef PARSER_PARSER
#define PARSER_PARSER

#include <cassert>
#include "common/Token.hpp"
#include "common/LocatedError.hpp"
#include "common/AST.hpp"

/// Try to chomp the token tag. ARREST if unsuccessful.
#define CHOMP_ELSE_ARREST(tokenTag, expected, element) if (p->tag == tokenTag) \
  ++p; else { errTryingToParse = element; expectedTokens = expected; \
  error = ARRESTING_ERR; return nullptr; }

/// Cause an epsilon error to occur.
#define EPSILON { error = EPSILON_ERR; return nullptr; }

/// If an error occurred, leave the error the same and return.
#define RETURN_IF_ERROR if (error != NOERROR) { return nullptr; }

/// If an error occurred, promote the error to an ARRESTING_ERR and return.
#define ARREST_IF_ERROR if (error != NOERROR) \
  {error = ARRESTING_ERR; return nullptr;}

/// @brief Return `r` following an arresting error or successful parse.
/// Otherwise reset `error` and continue this parse function. Used to implement
/// non-backtracking choice.
#define CONTINUE_ON_EPSILON(r) if (error != EPSILON_ERR) { return r; } else\
  { error = NOERROR; }


/// @brief The MiSCR parser. Builds an AST from a Token vector.
///
/// AST s are heap-allocated and should be destroyed via AST::deleteRecursive()
/// when no longer needed.
///
/// Parsing methods are named after the AST element that they parse (e.g.,
/// name(), exp(), decl()). If a parse is unsuccessful, the parsing function
/// returns nullptr and getError() returns a printable error message.
///
/// @note Currently, allocated AST memory is not freed if the parse fails.
class Parser {
  const std::vector<Token>& tokens;            // all the tokens
  std::vector<Token>::const_iterator p;        // current token

  /// Set when an error occurs.
  enum { NOERROR, EPSILON_ERR, ARRESTING_ERR } error = NOERROR;

  /// Set when an error occurs.
  const char* errTryingToParse = nullptr;

  /// Set when an error occurs.
  const char* expectedTokens = nullptr;

public:
  Parser(const std::vector<Token>& tokens)
    : tokens(tokens), p(tokens.begin()) {}

  /// @brief Returns the parser error after an unsuccessful parse.
  LocatedError getError() {
    LocatedError err('^');
    if (errTryingToParse != nullptr)
      { err << "I got stuck parsing " << errTryingToParse << "."; }
    else
      { err << "I got stuck while parsing.\n"; }
    if (expectedTokens != nullptr)
      { err << " I was expecting " << expectedTokens << " next.\n"; }
    err << p->loc;
    return err;
  }

  /// @brief True iff there are more (non-END) tokens to parse.
  bool hasMore() const { return p->tag != Token::END; }

  /// @brief Returns the first token that hasn't been parsed yet.
  Token getCurrentToken() const { return *p; }

  //==========================================================================//
  //=== Idents and QIdents
  //==========================================================================//

  /// @brief Parses an identifier (i.e., an unqualified name).
  Name* ident() {
    if (p->tag == Token::IDENT) return new Name(p->loc, (p++)->asString());
    EPSILON
  }

  /// @brief Same as ident() except the result is returned as an `std::string`.
  /// An empty string is returned on epsilon error.
  std::string namePart() {
    if (p->tag == Token::IDENT) return (p++)->asString();
    error = EPSILON_ERR;
    return "";
  }

  /// @brief Parses any Name, qualified or unqualified.
  Name* name() {
    Token begin = *p;
    std::string s = namePart(); RETURN_IF_ERROR
    while (chomp(Token::COLON_COLON)) {
      std::string s2 = namePart(); ARREST_IF_ERROR
      s.append("::");
      s.append(s2);
    }
    return new Name(hereFrom(begin), s);
  }

  //==========================================================================//
  //=== Expressions
  //==========================================================================//

  BoolLit* boolLit() {
    if (p->tag == Token::KW_FALSE) return new BoolLit((p++)->loc, false);
    if (p->tag == Token::KW_TRUE) return new BoolLit((p++)->loc, true);
    EPSILON
  }

  IntLit* intLit() {
    if (p->tag == Token::LIT_INT) return new IntLit(p->loc, (p++)->ptr);
    EPSILON
  }

  DecimalLit* decimalLit() {
    if (p->tag == Token::LIT_DEC) return new DecimalLit(p->loc, (p++)->ptr);
    EPSILON
  }

  StringLit* stringLit() {
    if (p->tag == Token::LIT_STRING) return new StringLit(p->loc, (p++)->ptr);
    EPSILON
  }

  Exp* parensExp() {
    if (!chomp(Token::LPAREN)) EPSILON
    Exp* ret = exp(); ARREST_IF_ERROR
    CHOMP_ELSE_ARREST(Token::RPAREN, ")", "parentheses expression")
    return ret;
  }

  /// @brief Parses a Name or CallExp. Combining these into a single parser
  /// function avoids backtracking.
  Exp* nameOrCallExp() {
    Token begin = *p;
    Name* n = name(); RETURN_IF_ERROR
    if (chomp(Token::LPAREN)) {
      ExpList* argList = expListWotc0();
      CHOMP_ELSE_ARREST(Token::RPAREN, ")", "function call")
      return new CallExp(hereFrom(begin), n, argList);
    } else {
      return new NameExp(n);
    }
  }

  BlockExp* blockExp() {
    Token begin = *p;
    if (!chomp(Token::LBRACE)) EPSILON
    ExpList* statements = stmts0(); ARREST_IF_ERROR
    CHOMP_ELSE_ARREST(Token::RBRACE, "}", "block expression")
    return new BlockExp(hereFrom(begin), statements);
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
    EPSILON
  }

  Exp* expLv1() {
    Token begin = *p;
    Exp* e = expLv0(); RETURN_IF_ERROR
    for (;;) {
      if (chomp(Token::EXCLAIM)) {
        e = new DerefExp(hereFrom(begin), e);
      } else if (chomp(Token::DOT)) {
        Name* fieldName = ident(); ARREST_IF_ERROR
        e = new ProjectExp(hereFrom(begin), e, fieldName, ProjectExp::DOT);
      } else if (chomp(Token::ARROW)) {
        Name* fieldName = ident(); ARREST_IF_ERROR
        e = new ProjectExp(hereFrom(begin), e, fieldName, ProjectExp::ARROW);
      } else if (chomp(Token::LBRACKET)) {
        if (chomp(Token::DOT)) {
          Name* fieldName = ident(); ARREST_IF_ERROR
          CHOMP_ELSE_ARREST(Token::RBRACKET, "]", "index field expression")
          e = new ProjectExp(hereFrom(begin),e,fieldName,ProjectExp::BRACKETS);
        } else {
          Exp* indexExp = exp(); ARREST_IF_ERROR
          CHOMP_ELSE_ARREST(Token::RBRACKET, "]", "index expression")
          e = new IndexExp(hereFrom(begin), e, indexExp);
        }
      } else return e;
    }
  }

  Exp* expLv2() {
    Token begin = *p;
    if (chomp(Token::AMP)) {
      Exp* e = expLv2(); ARREST_IF_ERROR
      return new RefExp(hereFrom(begin), e);
    } else if (chomp(Token::OP_SUB)) {
      Exp* e = expLv2(); ARREST_IF_ERROR
      return new UnopExp(hereFrom(begin), UnopExp::NEG, e);
    } else if (chomp(Token::TILDE)) {
      Exp* e = expLv2(); ARREST_IF_ERROR
      return new UnopExp(hereFrom(begin), UnopExp::NOT, e);
    } else {
      return expLv1();
    }
  }

  Exp* expLv3() {
    Token begin = *p;
    if (chomp(Token::KW_BORROW)) {
      Exp* e = expLv3(); ARREST_IF_ERROR
      return new BorrowExp(hereFrom(begin), e);
    } else if (chomp(Token::KW_MOVE)) {
      Exp* e = expLv3(); ARREST_IF_ERROR
      return new MoveExp(hereFrom(begin), e);
    } else {
      return expLv2();
    }
  }

  Exp* expLv4() {
    Token begin = *p;
    Exp* e = expLv3(); RETURN_IF_ERROR
    while (chomp(Token::COLON)) {
      TypeExp* tyAscrip = typeExp(); ARREST_IF_ERROR
      e = new AscripExp(hereFrom(begin), e, tyAscrip);
    }
    return e;
  }

  Exp* expLv5() {
    Token begin = *p;
    Exp* lhs = expLv4();
    RETURN_IF_ERROR
    while (p->tag == Token::OP_MUL || p->tag == Token::OP_DIV ||
           p->tag == Token::OP_MOD) {
      auto op = p->tag == Token::OP_MUL ? BinopExp::MUL :
                p->tag == Token::OP_DIV ? BinopExp::DIV :
                                          BinopExp::MOD ;
      ++p;
      Exp* rhs = expLv4(); ARREST_IF_ERROR
      lhs = new BinopExp(hereFrom(begin), op, lhs, rhs);
    }
    return lhs;
  }

  Exp* expLv6() {
    Token begin = *p;
    Exp* lhs = expLv5(); RETURN_IF_ERROR
    while (p->tag == Token::OP_ADD || p->tag == Token::OP_SUB) {
      auto op = (p->tag == Token::OP_ADD) ? BinopExp::ADD : BinopExp::SUB;
      ++p;
      Exp* rhs = expLv5(); ARREST_IF_ERROR
      lhs = new BinopExp(hereFrom(begin), op, lhs, rhs);
    }
    return lhs;
  }

  Exp* expLv7() {
    Token begin = *p;
    Exp* lhs = expLv6(); RETURN_IF_ERROR
    while (p->tag == Token::OP_EQ || p->tag == Token::OP_NE
    || p->tag == Token::OP_GE || p->tag == Token::OP_GT
    || p->tag == Token::OP_LE || p->tag == Token::OP_LT) {
      auto op = p->tag == Token::OP_EQ ? BinopExp::EQ :
                p->tag == Token::OP_NE ? BinopExp::NE :
                p->tag == Token::OP_GE ? BinopExp::GE :
                p->tag == Token::OP_GT ? BinopExp::GT :
                p->tag == Token::OP_LE ? BinopExp::LE :
                                         BinopExp::LT ;
      ++p;
      Exp* rhs = expLv6(); ARREST_IF_ERROR
      lhs = new BinopExp(hereFrom(begin), op, lhs, rhs);
    }
    return lhs;
  }

  Exp* expLv8() {
    Token begin = *p;
    Exp* lhs = expLv7(); RETURN_IF_ERROR
    while (p->tag == Token::AMP && (p+1)->tag == Token::AMP) {
      p += 2;
      Exp* rhs = expLv7(); ARREST_IF_ERROR
      lhs = new BinopExp(hereFrom(begin), BinopExp::AND, lhs, rhs);
    }
    return lhs;
  }

  Exp* expLv9() {
    Token begin = *p;
    Exp* lhs = expLv8(); RETURN_IF_ERROR
    while (chomp(Token::OP_OR)) {
      Exp* rhs = expLv8(); ARREST_IF_ERROR
      lhs = new BinopExp(hereFrom(begin), BinopExp::OR, lhs, rhs);
    }
    return lhs;
  }

  Exp* expLv10() {
    Token begin = *p;
    Exp* lhs = expLv9(); RETURN_IF_ERROR
    while (chomp(Token::COLON_EQUAL)) {
      Exp* rhs = expLv9(); ARREST_IF_ERROR
      lhs = new StoreExp(hereFrom(begin), lhs, rhs);
    }
    return lhs;
  }

  IfExp* ifExp() {
    Token begin = *p;
    if (!chomp(Token::KW_IF)) EPSILON
    Exp* condExp = exp(); ARREST_IF_ERROR
    CHOMP_ELSE_ARREST(Token::KW_THEN, "then", "if expression")
    Exp* thenExp = exp(); ARREST_IF_ERROR
    CHOMP_ELSE_ARREST(Token::KW_ELSE, "else", "if expression")
    Exp* elseExp = exp(); ARREST_IF_ERROR
    return new IfExp(hereFrom(begin), condExp, thenExp, elseExp);
  }

  Exp* exp() {
    Exp* ret = ifExp(); CONTINUE_ON_EPSILON(ret)
    return expLv10();
  }

  /// @brief Zero or more comma-separated expressions with optional trailing
  /// comma (wotc). Never epsilon fails.
  ExpList* expListWotc0() {
    Token begin = *p;
    llvm::SmallVector<Exp*, 4> es;
    Exp* e;
    for (;;) {
      e = exp();
      if (error == ARRESTING_ERR) return nullptr;
      if (error == EPSILON_ERR) {
        error = NOERROR;
        return new ExpList(hereFrom(begin), es);
      }
      es.push_back(e);
      if (p->tag != Token::COMMA) {
        return new ExpList(hereFrom(begin), es);
      }
      ++p;
    }
  }

  //==========================================================================//
  //=== Type Expressions
  //==========================================================================//

  TypeExp* typeExpLv0() {
    if (p->tag == Token::KW_f32)
      return new PrimitiveTypeExp((p++)->loc, PrimitiveTypeExp::f32);
    if (p->tag == Token::KW_f64)
      return new PrimitiveTypeExp((p++)->loc, PrimitiveTypeExp::f64);
    if (p->tag == Token::KW_i8)
      return new PrimitiveTypeExp((p++)->loc, PrimitiveTypeExp::i8);
    if (p->tag == Token::KW_i16)
      return new PrimitiveTypeExp((p++)->loc, PrimitiveTypeExp::i16);
    if (p->tag == Token::KW_i32)
      return new PrimitiveTypeExp((p++)->loc, PrimitiveTypeExp::i32);
    if (p->tag == Token::KW_i64)
      return new PrimitiveTypeExp((p++)->loc, PrimitiveTypeExp::i64);
    if (p->tag == Token::KW_BOOL)
      return new PrimitiveTypeExp((p++)->loc, PrimitiveTypeExp::BOOL);
    if (p->tag == Token::KW_UNIT)
       return new PrimitiveTypeExp((p++)->loc, PrimitiveTypeExp::UNIT);
    TypeExp* ret;
    ret = nameTypeExp(); CONTINUE_ON_EPSILON(ret)
    EPSILON
  }

  TypeExp* typeExp() {
    Token begin = *p;
    if (p->tag == Token::AMP || p->tag == Token::HASH) {
      bool owned = p->tag == Token::HASH;
      ++p;
      TypeExp* n1 = typeExp(); ARREST_IF_ERROR
      return new RefTypeExp(hereFrom(begin), n1, owned);
    } else {
      return typeExpLv0();
    }
  }

  NameTypeExp* nameTypeExp() {
    Name* n = name(); RETURN_IF_ERROR
    return new NameTypeExp(n);
  }

  //==========================================================================//
  //=== Statements
  //==========================================================================//

  LetExp* letStmt() {
    Token begin = *p;
    if (!chomp(Token::KW_LET)) EPSILON
    Name* boundIdent = ident(); ARREST_IF_ERROR
    TypeExp* ascrip = nullptr;
    if (chomp(Token::COLON)) {
      ascrip = typeExp(); ARREST_IF_ERROR
      CHOMP_ELSE_ARREST(Token::EQUAL, "=", "let statement")
    } else {
      CHOMP_ELSE_ARREST(Token::EQUAL, ": =", "let statement")
    }
    Exp* definition = exp(); ARREST_IF_ERROR
    return new LetExp(hereFrom(begin), boundIdent, ascrip, definition);
  }

  ReturnExp* returnStmt() {
    Token begin = *p;
    if (!chomp(Token::KW_RETURN)) EPSILON
    Exp* returnee = exp(); ARREST_IF_ERROR
    CHOMP_ELSE_ARREST(Token::SEMICOLON, ";", "return statement")
    return new ReturnExp(hereFrom(begin), returnee);
  }

  Exp* stmt() {
    Exp* ret;
    ret = letStmt(); CONTINUE_ON_EPSILON(ret)
    ret = returnStmt(); CONTINUE_ON_EPSILON(ret)
    ret = exp(); CONTINUE_ON_EPSILON(ret)
    EPSILON
  }

  /// Zero or more statements separated by semicolons Never epsilon fails.
  ExpList* stmts0() {
    Token begin = *p;
    llvm::SmallVector<Exp*, 4> ss;
    Exp* s;
    for (;;) {
      s = stmt();
      if (error == ARRESTING_ERR) return nullptr;
      if (error == EPSILON_ERR) {
        error = NOERROR;
        return new ExpList(hereFrom(begin), ss);
      }
      ss.push_back(s);
      if (!chomp(Token::SEMICOLON)) {
        return new ExpList(hereFrom(begin), ss);
      }
    }
  }

  //==========================================================================//
  //=== Declarations
  //==========================================================================//

  /// @brief Parses parameter list of length zero or higher with optional
  /// terminal comma (wotc). This parser never epsilon errors.
  ParamList* paramListWotc0() {
    Token begin = *p;
    llvm::SmallVector<std::pair<Name*, TypeExp*>, 4> params;
    Name* paramName;
    TypeExp* paramType;
    for (;;) {
      paramName = ident();
      if (error == ARRESTING_ERR) return nullptr;
      if (error == EPSILON_ERR) {
        error = NOERROR;
        return new ParamList(hereFrom(begin), params);
      }
      CHOMP_ELSE_ARREST(Token::COLON, ":", "parameter list")
      paramType = typeExp(); ARREST_IF_ERROR
      params.push_back(std::pair<Name*, TypeExp*>(paramName, paramType));
      if (p->tag != Token::COMMA) {
        return new ParamList(hereFrom(begin), params);
      }
      ++p;
    }
  }

  DataDecl* dataDecl() {
    Token begin = *p;
    if (!chomp(Token::KW_DATA)) EPSILON
    Name* name = ident(); ARREST_IF_ERROR
    CHOMP_ELSE_ARREST(Token::LPAREN, "(", "data")
    ParamList* fields = paramListWotc0(); ARREST_IF_ERROR
    CHOMP_ELSE_ARREST(Token::RPAREN, ")", "data")
    return new DataDecl(hereFrom(begin), name, fields);
  }

  FunctionDecl* functionDecl() {
    Token begin = *p;
    bool hasBody = true;
    bool variadic = false;
    if (chomp(Token::KW_EXTERN)) {
      hasBody = false;
      CHOMP_ELSE_ARREST(Token::KW_FUNC, "func", "function")
    } else if (!chomp(Token::KW_FUNC)) EPSILON
    Name* name = ident(); ARREST_IF_ERROR
    CHOMP_ELSE_ARREST(Token::LPAREN, "(", "function")
    ParamList* params = paramListWotc0(); ARREST_IF_ERROR
    if (!hasBody) {
      if (chomp(Token::ELLIPSIS)) variadic = true;
    }
    CHOMP_ELSE_ARREST(Token::RPAREN, ")", "function")
    CHOMP_ELSE_ARREST(Token::COLON, ":", "function")
    TypeExp* retType = typeExp(); ARREST_IF_ERROR
    if (hasBody) {
      CHOMP_ELSE_ARREST(Token::EQUAL, "=", "function")
      Exp* body = exp(); ARREST_IF_ERROR
      CHOMP_ELSE_ARREST(Token::SEMICOLON, ";", "function")
      return new FunctionDecl(hereFrom(begin), name, params, retType, body);
    } else {
      CHOMP_ELSE_ARREST(Token::SEMICOLON, ";", "function")
      return new FunctionDecl(hereFrom(begin), name, params, retType, nullptr,
        variadic);
    }
  }

  ModuleDecl* module_() {
    Token begin = *p;
    if (!chomp(Token::KW_MODULE)) EPSILON
    Name* moduleName = ident(); ARREST_IF_ERROR
    CHOMP_ELSE_ARREST(Token::LBRACE, "{", "module")
    DeclList* n2 = decls0(); ARREST_IF_ERROR
    CHOMP_ELSE_ARREST(Token::RBRACE, "}", "module")
    return new ModuleDecl(hereFrom(begin), moduleName, n2);
  }

  Decl* decl() {
    Decl* ret;
    ret = dataDecl(); CONTINUE_ON_EPSILON(ret)
    ret = functionDecl(); CONTINUE_ON_EPSILON(ret)
    ret = module_(); CONTINUE_ON_EPSILON(ret)
    EPSILON
  }

  /// @brief Parses zero or more declarations.
  DeclList* decls0() {
    Token begin = *p;
    llvm::SmallVector<Decl*, 0> ds;
    Decl* d;
    for (;;) {
      d = decl();
      if (error == ARRESTING_ERR) return nullptr;
      if (error == EPSILON_ERR) {
        error = NOERROR;
        return new DeclList(hereFrom(begin), ds);
      }
      ds.push_back(d);
    }
  }

private:

  /// @brief Consumes a token of type @p tag if possible, otherwise consumes
  /// nothing. `error` is _not_ set.
  /// @return True iff @p tag was consumed.
  bool chomp(Token::Tag tag) {
    if (p->tag == tag)
      { ++p; return true; }
    else
      { return false; }
  }

  /// @brief Returns the location that spans from the beginning of @p first to
  /// the end of the token before the current one. Make sure there _is_ a
  /// previous token before calling this function.
  Location hereFrom(Token first) {
    return Location(first.loc.row, first.loc.col,
      (p-1)->ptr - first.ptr + (p-1)->loc.sz);
  }
};

#endif