#ifndef PARSER_PARSER
#define PARSER_PARSER

#include <cassert>
#include "common/Token.hpp"
#include "common/ASTContext.hpp"
#include "common/LocatedError.hpp"
#include "common/AST.hpp"

#define CHOMP_ELSE_ARREST(tokenTy, expected, element) if (p->ty == tokenTy) \
  ++p; else { errTryingToParse = element; expectedTokens = expected; \
  return ARRESTING_ERROR; }

/// If the address is an error, return the error.
#define RETURN_IF_ERROR(a) if (a.isEpsilon()) \
  { return EPSILON_ERROR; } else if (a.isArrest()) \
  { return ARRESTING_ERROR; }

/// If the address is an error, return an arresting error.
#define ARREST_IF_ERROR(a) if (a.isError()) { return ARRESTING_ERROR; }


class Parser {
  ASTContext* ctx;
  const std::vector<Token>* tokens;
  std::vector<Token>::const_iterator p;
  std::vector<Token>::const_iterator end;

  /// Set when an error occurs.
  const char* errTryingToParse = nullptr;

  /// Set when an error occurs.
  const char* expectedTokens = nullptr;

public:
  Parser(ASTContext* ctx, const std::vector<Token>* _tokens) {
    this->ctx = ctx;
    tokens = _tokens;
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

  Addr<Ident> ident() {
    if (p->ty == TOK_IDENT) return ctx->add(Ident(tokToLoc(p), (p++)->ptr));
    return EPSILON_ERROR;
  }

  /// Parses a name (either IDENT or QIDENT).
  Addr<Name> name() {
    Token t = *p;
    Addr<Ident> head = ident();
    if (head.isError()) return head.upcast<Name>();
    if (p->ty == COLON_COLON) {
      p++;
      Addr<Name> tail = name();
      if (tail.isError()) return ARRESTING_ERROR;
      Location loc(t.row, t.col, (unsigned int)(p->ptr - t.ptr));
      return ctx->add(QIdent(loc, head, tail)).upcast<Name>();
    } else {
      return head.upcast<Name>();
    }
  }

  // ---------- Expressions ----------

  Addr<BoolLit> boolLit() {
    if (p->ty == KW_FALSE) return ctx->add(BoolLit(tokToLoc(p++), false));
    if (p->ty == KW_TRUE) return ctx->add(BoolLit(tokToLoc(p++), true));
    return EPSILON_ERROR;
  }

  Addr<IntLit> intLit() {
    if (p->ty != LIT_INT) return EPSILON_ERROR;
    auto ret = ctx->add(IntLit(tokToLoc(p), p->ptr));
    ++p;
    return ret;
  }

  Addr<DecimalLit> decimalLit() {
    if (p->ty != LIT_DEC) return EPSILON_ERROR;
    auto ret = ctx->add(DecimalLit(tokToLoc(p), p->ptr));
    ++p;
    return ret;
  }

  Addr<StringLit> stringLit() {
    if (p->ty != LIT_STRING) return EPSILON_ERROR;
    auto ret = ctx->add(StringLit(tokToLoc(p), p->ptr));
    ++p;
    return ret;
  }

  Addr<Exp> parensExp() {
    if (p->ty == LPAREN) ++p; else return EPSILON_ERROR;
    Addr<Exp> ret = exp();
    if (ret.isError()) return ARRESTING_ERROR;
    CHOMP_ELSE_ARREST(RPAREN, ")", "parentheses expression")
    return ret;
  }

  Addr<Exp> nameOrCallExp() {
    Token t = *p;
    Addr<Name> n = name();
    RETURN_IF_ERROR(n)
    if (p->ty == LPAREN) {
      ++p;
      Addr<ExpList> argList = expListWotc0();
      CHOMP_ELSE_ARREST(RPAREN, ")", "function call")
      Location loc(t.row, t.col, (unsigned int)(p->ptr - t.ptr));
      return ctx->add(CallExp(loc, n, argList)).upcast<Exp>();
    } else {
      return ctx->add(NameExp(ctx->get(n).getLocation(), n)).upcast<Exp>();
    }
  }

  Addr<BlockExp> blockExp() {
    Token t = *p;
    if (p->ty == LBRACE) ++p; else return EPSILON_ERROR;
    Addr<ExpList> statements = stmts0();
    ARREST_IF_ERROR(statements)
    CHOMP_ELSE_ARREST(RBRACE, "}", "block expression")
    Location loc(t.row, t.col, (unsigned int)(p->ptr - t.ptr));
    return ctx->add(BlockExp(loc, statements));
  }

  Addr<Exp> expLv0() {
    Addr<Exp> ret;
    if ((ret = nameOrCallExp()).notEpsilon()) return ret;
    if ((ret = boolLit().upcast<Exp>()).notEpsilon()) return ret;
    if ((ret = intLit().upcast<Exp>()).notEpsilon()) return ret;
    if ((ret = decimalLit().upcast<Exp>()).notEpsilon()) return ret;
    if ((ret = stringLit().upcast<Exp>()).notEpsilon()) return ret;
    if ((ret = parensExp()).notEpsilon()) return ret;
    // if ((ret = arrayConstrExp().upcast<Exp>()).notEpsilon()) return ret;
    if ((ret = blockExp().upcast<Exp>()).notEpsilon()) return ret;
    return EPSILON_ERROR;
  }

  Addr<Exp> expLv1() {
    Token t = *p;
    if (t.ty == AMP || t.ty == HASH) {
      ++p;
      Addr<Exp> e = expLv1();
      ARREST_IF_ERROR(e)
      Location loc(t.row, t.col, (unsigned int)(p->ptr - t.ptr));
      return t.ty == HASH ?
             ctx->add(WRefExp(loc, e)).upcast<Exp>() :
             ctx->add(RefExp(loc, e)).upcast<Exp>();
    } else {
      return expLv0();
    }
  }

  Addr<Exp> expLv2() {
    Token t = *p;
    Addr<Exp> e = expLv1();
    RETURN_IF_ERROR(e)
    while (p->ty == COLON) {
      ++p;
      Addr<TypeExp> tyAscrip = typeExp();
      ARREST_IF_ERROR(tyAscrip)
      Location loc(t.row, t.col, (unsigned int)(p->ptr - t.ptr));
      e = ctx->add(AscripExp(loc, e, tyAscrip)).upcast<Exp>();
    }
    return e;
  }

  Addr<Exp> expLv3() {
    Token t = *p;
    Addr<Exp> lhs = expLv2();
    RETURN_IF_ERROR(lhs)
    while (p->ty == OP_MUL || p->ty == OP_DIV) {
      AST::ID op = (p->ty == OP_MUL) ? AST::ID::MUL : AST::ID::DIV;
      ++p;
      Addr<Exp> rhs = expLv2();
      ARREST_IF_ERROR(rhs)
      Location loc(t.row, t.col, (unsigned int)(p->ptr - t.ptr));
      lhs = ctx->add(BinopExp(op, loc, lhs, rhs)).upcast<Exp>();
    }
    return lhs;
  }

  Addr<Exp> expLv4() {
    Token t = *p;
    Addr<Exp> lhs = expLv3();
    RETURN_IF_ERROR(lhs)
    while (p->ty == OP_ADD || p->ty == OP_SUB) {
      AST::ID op = (p->ty == OP_ADD) ? AST::ID::ADD : AST::ID::SUB;
      ++p;
      Addr<Exp> rhs = expLv3();
      ARREST_IF_ERROR(rhs)
      Location loc(t.row, t.col, (unsigned int)(p->ptr - t.ptr));
      lhs = ctx->add(BinopExp(op, loc, lhs, rhs)).upcast<Exp>();
    }
    return lhs;
  }

  Addr<Exp> expLv5() {
    Token t = *p;
    Addr<Exp> lhs = expLv4();
    RETURN_IF_ERROR(lhs)
    while (p->ty == OP_EQ || p->ty == OP_NEQ) {
      AST::ID op = (p->ty == OP_EQ) ? AST::ID::EQ : AST::ID::NE;
      ++p;
      Addr<Exp> rhs = expLv4();
      ARREST_IF_ERROR(rhs)
      Location loc(t.row, t.col, (unsigned int)(p->ptr - t.ptr));
      lhs = ctx->add(BinopExp(op, loc, lhs, rhs)).upcast<Exp>();
    }
    return lhs;
  }

  Addr<IfExp> ifExp() {
    Token t = *p;
    if (p->ty == KW_IF) ++p; else return EPSILON_ERROR;
    Addr<Exp> condExp = exp();
    ARREST_IF_ERROR(condExp)
    CHOMP_ELSE_ARREST(KW_THEN, "then", "if expression")
    Addr<Exp> thenExp = exp();
    ARREST_IF_ERROR(thenExp)
    CHOMP_ELSE_ARREST(KW_ELSE, "else", "if expression")
    Addr<Exp> elseExp = exp();
    ARREST_IF_ERROR(elseExp)
    Location loc(t.row, t.col, (unsigned int)(p->ptr - t.ptr));
    return ctx->add(IfExp(loc, condExp, thenExp, elseExp));
  }

  Addr<Exp> exp() {
    Addr<Exp> ret = ifExp().upcast<Exp>();
    if (ret.notEpsilon()) return ret;
    return expLv5();
  }

  /// Zero or more comma-separated expressions with optional trailing comma
  /// (wotc). Never epsilon fails.
  Addr<ExpList> expListWotc0() {
    Token t = *p;
    Addr<Exp> head = exp();
    if (head.isArrest()) return ARRESTING_ERROR;
    if (head.isEpsilon()) return ctx->add(ExpList(Location(t.row, t.col, 0)));
    Addr<ExpList> tail;
    if (p->ty == COMMA) {
      ++p;
      tail = expListWotc0();
      ARREST_IF_ERROR(tail)
    } else {
      tail = ctx->add(ExpList(Location(p->row, p->col, 0)));
    }
    Location loc(t.row, t.col, (unsigned int)(p->ptr - t.ptr));
    return ctx->add(ExpList(loc, head, tail));
  }

  // ---------- Type Expressions ----------

  Addr<TypeExp> typeExpLv0() {
    if (p->ty == KW_f32) return ctx->add(TypeExp::mkF32(tokToLoc(p++)));
    if (p->ty == KW_f64) return ctx->add(TypeExp::mkF64(tokToLoc(p++)));
    if (p->ty == KW_i8) return ctx->add(TypeExp::mkI8(tokToLoc(p++)));
    if (p->ty == KW_i32) return ctx->add(TypeExp::mkI32(tokToLoc(p++)));
    if (p->ty == KW_BOOL) return ctx->add(TypeExp::mkBool(tokToLoc(p++)));
    // if (p->ty == KW_STRING) return ctx->add(TypeExp::mkString(tokToLoc(p++)));
    if (p->ty == KW_UNIT) return ctx->add(TypeExp::mkUnit(tokToLoc(p++)));
    // Addr<TypeExp> ret = arrayType();
    // if (ret.notEpsilon()) return ret;
    return EPSILON_ERROR;
  }

  Addr<TypeExp> typeExp() {
    Token t = *p;
    if (t.ty == AMP || t.ty == HASH) {
      ++p;
      Addr<TypeExp> n1 = typeExp();
      ARREST_IF_ERROR(n1)
      Location loc(t.row, t.col, (unsigned int)(p->ptr - t.ptr));
      return t.ty == HASH ?
             ctx->add(WRefTypeExp(loc, n1)).upcast<TypeExp>() :
             ctx->add(RefTypeExp(loc, n1)).upcast<TypeExp>();
    } else {
      return typeExpLv0();
    }
  }

  // Addr<TypeExp> arrayType() {
  //   Token t = *p;
  //   if (p->ty == LBRACKET) ++p; else return EPSILON_ERROR;
  //   unsigned int n1 = intLit();
  //   if (IS_ERROR(n1)) return ARRESTING_ERROR;
  //   CHOMP_ELSE_ARREST(KW_OF, "of", "array type")
  //   unsigned int n2 = tyExp();
  //   if (IS_ERROR(n2)) return ARRESTING_ERROR;
  //   CHOMP_ELSE_ARREST(RBRACKET, "]", "array type")
  //   Location loc = { t.row, t.col, (unsigned int)(p->ptr - t.ptr) };
  //   return m->add({ loc, n1, n2, NN, NOEXTRA, NodeTy::TY_ARRAY });
  // }

  // ---------- Statements ----------

  Addr<LetExp> letStmt() {
    Token t = *p;
    if (p->ty == KW_LET) p++; else return EPSILON_ERROR;
    Addr<Ident> boundIdent = ident();
    ARREST_IF_ERROR(boundIdent)
    CHOMP_ELSE_ARREST(EQUAL, "=", "let statement")
    Addr<Exp> definition = exp();
    ARREST_IF_ERROR(definition)
    Location loc(t.row, t.col, (unsigned int)(p->ptr - t.ptr));
    return ctx->add(LetExp(loc, boundIdent, definition));
  }

  Addr<ReturnExp> returnStmt() {
    Token t = *p;
    if (p->ty == KW_RETURN) ++p; else return EPSILON_ERROR;
    Addr<Exp> returnee = exp();
    ARREST_IF_ERROR(returnee)
    CHOMP_ELSE_ARREST(SEMICOLON, ";", "return statement")
    Location loc(t.row, t.col, (unsigned int)(p->ptr - t.ptr));
    return ctx->add(ReturnExp(loc, returnee));
  }

  Addr<Exp> stmt() {
    Addr<Exp> ret;
    if ((ret = letStmt().upcast<Exp>()).notEpsilon()) return ret;
    if ((ret = returnStmt().upcast<Exp>()).notEpsilon()) return ret;
    if ((ret = exp().upcast<Exp>()).notEpsilon()) return ret;
    return ret;
  }

  /// Zero or more statements separated by semicolons Never epsilon fails.
  Addr<ExpList> stmts0() {
    Token t = *p;
    Addr<Exp> head = stmt();
    if (head.isArrest()) return ARRESTING_ERROR;
    if (head.isEpsilon()) return ctx->add(ExpList(Location(t.row, t.col, 0)));
    Addr<ExpList> tail;
    if (p->ty == SEMICOLON) {
      ++p;
      tail = stmts0();
      ARREST_IF_ERROR(tail)
    } else {
      tail = ctx->add(ExpList(Location(p->row, p->col, 0)));
    }
    Location loc(t.row, t.col, (unsigned int)(p->ptr - t.ptr));
    return ctx->add(ExpList(loc, head, tail));
  }

  // ---------- Declarations ----------

  /// Parses parameter list of length zero or higher with optional terminal
  /// comma (wotc). This parser never returns an epsilon error.
  Addr<ParamList> paramListWotc0() {
    Token t = *p;
    Addr<Ident> paramName = ident();
    if (paramName.isArrest())
      return ARRESTING_ERROR;
    if (paramName.isEpsilon())
      return ctx->add(ParamList(Location(t.row, t.col, 0)));
    CHOMP_ELSE_ARREST(COLON, ":", "parameter list")
    Addr<TypeExp> paramType = typeExp();
    ARREST_IF_ERROR(paramType)
    Addr<ParamList> tail;
    if (p->ty == COMMA) { ++p; tail = paramListWotc0(); }
    else tail = ctx->add(ParamList(Location(p->row, p->col, 0)));
    Location loc(t.row, t.col, (unsigned int)(p->ptr - t.ptr));
    return ctx->add(ParamList(loc, paramName, paramType, tail));
  }

  Addr<FunctionDecl> functionDecl() {
    Token t = *p;
    if (p->ty == KW_FUNC) ++p; else return EPSILON_ERROR;
    Addr<Name> name = ident().upcast<Name>();
    ARREST_IF_ERROR(name)
    CHOMP_ELSE_ARREST(LPAREN, "(", "function")
    Addr<ParamList> params = paramListWotc0();
    if (params.isError()) return ARRESTING_ERROR;
    CHOMP_ELSE_ARREST(RPAREN, ")", "function")
    CHOMP_ELSE_ARREST(COLON, ":", "function")
    Addr<TypeExp> retType = typeExp();
    ARREST_IF_ERROR(retType)
    CHOMP_ELSE_ARREST(EQUAL, "=", "function")
    Addr<Exp> body = exp();
    ARREST_IF_ERROR(body)
    CHOMP_ELSE_ARREST(SEMICOLON, ";", "function")
    Location loc(t.row, t.col, (unsigned int)(p->ptr - t.ptr));
    return ctx->add(FunctionDecl(loc, name, params, retType, body));
  }

  /** Parses a module or namespace with brace-syntax. */
  Addr<Decl> moduleOrNamespace() {
    Token t = *p;
    AST::ID ty;
    if (p->ty == KW_MODULE) { ty = AST::ID::MODULE; ++p; }
    else if (p->ty == KW_NAMESPACE) { ty = AST::ID::NAMESPACE; ++p; }
    else return EPSILON_ERROR;
    const char* whatThisIs = ty == AST::ID::MODULE ? "module" : "namespace";
    Addr<Name> n1 = ident().upcast<Name>();
    ARREST_IF_ERROR(n1)
    CHOMP_ELSE_ARREST(LBRACE, "{", whatThisIs)
    Addr<DeclList> n2 = decls0();
    ARREST_IF_ERROR(n2)
    CHOMP_ELSE_ARREST(RBRACE, "}", whatThisIs)
    Location loc(t.row, t.col, (unsigned int)(p->ptr - t.ptr));
    return ty == AST::ID::MODULE ?
           ctx->add(ModuleDecl(loc, n1, n2)).upcast<Decl>() :
           ctx->add(NamespaceDecl(loc, n1, n2)).upcast<Decl>();
  }

  /** A declaration is a func, proc, data type, module, or namespace. */
  Addr<Decl> decl() {
    Addr<Decl> ret;
    if ((ret = functionDecl().upcast<Decl>()).notEpsilon()) return ret;
    if ((ret = moduleOrNamespace()).notEpsilon()) return ret;
    return EPSILON_ERROR;
  }

  /// Parses zero or more declarations into a DECLLIST.
  Addr<DeclList> decls0() {
    Token t = *p;
    Addr<Decl> head = decl();
    if (head.isArrest()) return ARRESTING_ERROR;
    if (head.isEpsilon()) return ctx->add(DeclList(Location(t.row, t.col, 0)));
    Addr<DeclList> tail = decls0();
    ARREST_IF_ERROR(tail)
    Location loc(t.row, t.col, (unsigned int)(p->ptr - t.ptr));
    return ctx->add(DeclList(loc, head, tail));
  }

  // ----------

  Location tokToLoc(std::vector<Token>::const_iterator tok) {
    return Location(tok->row, tok->col, tok->sz);
  }
};

#endif