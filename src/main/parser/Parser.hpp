#ifndef PARSER_PARSER
#define PARSER_PARSER

#include <cstdio>
#include <vector>
#include "common/NodeManager.hpp"

#define ARRESTING_ERROR 0xffffffff
#define EPSILON_ERROR   0xfffffffe
#define IS_ERROR(x)     ((x) >= 0xfffffffe)

class Parser {

public:
  std::vector<Token> tokens;
  std::vector<Token>::iterator p;
  std::vector<Token>::iterator end;
  NodeManager m;
  long nextUnusedTypeVarID = 1;

  Parser(std::vector<Token> &_tokens) {
    tokens = _tokens;
    p = tokens.begin();
    end = tokens.end();
  }

  unsigned int numLit() {
    if (p->ty != LIT_INT && p->ty != LIT_DEC) return EPSILON_ERROR;
    char buffer[p->sz + 1];
    strncpy(buffer, p->ptr, p->sz);
    buffer[p->sz] = '\0';
    if (p->ty == LIT_INT) {
      long value = atol(buffer);
      return m.add({ tokenToLocation(p++), newTYPEVAR(), NN, NN, { .intVal=value }, NodeTy::LIT_INT });
    } else if (p->ty == LIT_DEC) {
      double value = atof(buffer);
      return m.add({ tokenToLocation(p++), newTYPEVAR(), NN, NN, { .decVal=value }, NodeTy::LIT_DEC });
    } else return EPSILON_ERROR;
  }

  unsigned int litString() {
    if (p->ty == LIT_STRING) return m.add({ tokenToLocation(p), newTYPEVAR(), NN, NN, { .ptr=(p++)->ptr }, NodeTy::LIT_STRING });
    else return EPSILON_ERROR;
  }

  unsigned int litBool() {
    if (p->ty == KW_FALSE) return m.add({ tokenToLocation(p), newTYPEVAR(), NN, NN, { .ptr=(p++)->ptr }, NodeTy::FALSE });
    else if (p->ty == KW_TRUE) return m.add({ tokenToLocation(p), newTYPEVAR(), NN, NN, { .ptr=(p++)->ptr }, NodeTy::TRUE });
    else return EPSILON_ERROR;
  }

  unsigned int ident() {
    if (p->ty == TOK_IDENT) return m.add({ tokenToLocation(p), NN, NN, NN, { .ptr=(p++)->ptr }, NodeTy::IDENT });
    return EPSILON_ERROR;
  }

  unsigned int eident() {
    if (p->ty == TOK_IDENT) return m.add({ tokenToLocation(p), newTYPEVAR(), NN, NN, { .ptr=(p++)->ptr }, NodeTy::EIDENT });
    return EPSILON_ERROR;
  }

  unsigned int parensExp() {
    if (p->ty == LPAREN) p++; else return EPSILON_ERROR;
    unsigned int e = exp();
    if (IS_ERROR(e)) return ARRESTING_ERROR;
    if (p->ty == RPAREN) p++; else return ARRESTING_ERROR;
    return e;
  }

  unsigned int blockExp() {
    Token t = *p;
    if (p->ty == LBRACE) p++; else return EPSILON_ERROR;
    unsigned int n = stmts0();
    if (IS_ERROR(n)) return ARRESTING_ERROR;
    if (p->ty == RBRACE) p++; else return EPSILON_ERROR;
    Location loc = { t.row, t.col, (unsigned int)(p->ptr - t.ptr) };
    return m.add({ loc, newTYPEVAR(), n, NN, NOEXTRA, NodeTy::BLOCK });
  }

  unsigned int eidentOrFunctionCall() {
    Token t = *p;
    unsigned int n2 = eident();
    if (IS_ERROR(n2)) return n2;
    if (p->ty == LPAREN) {
      p++;
      unsigned int n3 = expListWotc0();
      if (p->ty == RPAREN) p++; else return ARRESTING_ERROR;
      Location loc = { t.row, t.col, (unsigned int)(p->ptr - t.ptr) };
      return m.add({ loc, newTYPEVAR(), n2, n3, NOEXTRA, NodeTy::CALL });
    } else {
      return n2;
    }
  }

  unsigned int expLv0() {
    unsigned int n1;
    n1 = eidentOrFunctionCall();
    if (n1 != EPSILON_ERROR) return n1;
    n1 = numLit();
    if (n1 != EPSILON_ERROR) return n1;
    n1 = litString();
    if (n1 != EPSILON_ERROR) return n1;
    n1 = litBool();
    if (n1 != EPSILON_ERROR) return n1;
    n1 = parensExp();
    if (n1 != EPSILON_ERROR) return n1;
    return blockExp();
  }

  unsigned int expLv1() {
    Token t = *p;
    unsigned int e = expLv0();
    if (IS_ERROR(e)) return e;
    while (p->ty == OP_MUL || p->ty == OP_DIV) {
      NodeTy op = (p->ty == OP_MUL) ? NodeTy::MUL : NodeTy::DIV;
      p++;
      unsigned int e2 = expLv0();
      if (IS_ERROR(e2)) return ARRESTING_ERROR;
      Location loc = { t.row, t.col, (unsigned int)(p->ptr - t.ptr) };
      e = m.add({ loc, newTYPEVAR(), e, e2, NOEXTRA, op });
    }
    return e;
  }

  unsigned int expLv2() {
    Token t = *p;
    unsigned int e = expLv1();
    if (IS_ERROR(e)) return e;
    while (p->ty == OP_ADD || p->ty == OP_SUB) {
      NodeTy op = (p->ty == OP_ADD) ? NodeTy::ADD : NodeTy::SUB;
      p++;
      unsigned int e2 = expLv1();
      if (IS_ERROR(e2)) return ARRESTING_ERROR;
      Location loc = { t.row, t.col, (unsigned int)(p->ptr - t.ptr) };
      e = m.add({ loc, newTYPEVAR(), e, e2, NOEXTRA, op });
    }
    return e;
  }

  unsigned int expLv3() {
    Token t = *p;
    unsigned int e = expLv2();
    if (IS_ERROR(e)) return e;
    while (p->ty == OP_EQ || p->ty == OP_NEQ) {
      NodeTy op = (p->ty == OP_EQ) ? NodeTy::EQ : NodeTy::NE;
      p++;
      unsigned int e2 = expLv2();
      if (IS_ERROR(e2)) return ARRESTING_ERROR;
      Location loc = { t.row, t.col, (unsigned int)(p->ptr - t.ptr) };
      e = m.add({ loc, newTYPEVAR(), e, e2, NOEXTRA, op });
    }
    return e;
  }

  unsigned int ifExp() {
    Token t = *p;
    if (p->ty == KW_IF) p++; else return EPSILON_ERROR;
    unsigned int n2 = exp();
    if (IS_ERROR(n2)) return ARRESTING_ERROR;
    if (p->ty == KW_THEN) p++; else return ARRESTING_ERROR;
    unsigned int n3 = exp();
    if (IS_ERROR(n3)) return ARRESTING_ERROR;
    if (p->ty == KW_ELSE) p++; else return ARRESTING_ERROR;
    unsigned int n4 = exp();
    if (IS_ERROR(n4)) return ARRESTING_ERROR;
    Location loc = { t.row, t.col, (unsigned int)(p->ptr - t.ptr) };
    return m.add({ loc, newTYPEVAR(), n2, n3, { .nodes={ n4, NN } }, NodeTy::IF });
  }

  unsigned int exp() {
    unsigned int n = ifExp();
    if (n != EPSILON_ERROR) return n;
    return expLv3();
  }

  /** Zero or more comma-separated expressions with optional trailing
   * comma (wotc). Never epsilon fails. */
  unsigned int expListWotc0() {
    Token t = *p;
    unsigned int n1 = exp();
    if (n1 == ARRESTING_ERROR) return ARRESTING_ERROR;
    if (n1 == EPSILON_ERROR) {
      Location loc = { t.row, t.col, 0 };
      return m.add({ loc, NN, NN, NN, NOEXTRA, NodeTy::EXPLIST_NIL });
    }
    unsigned int n2;
    if (p->ty == COMMA) {
      p++;
      n2 = expListWotc0();
      if (IS_ERROR(n2)) return ARRESTING_ERROR;
    } else {
      n2 = m.add({ { p->row, p->col, 0 }, NN, NN, NN, NOEXTRA, NodeTy::EXPLIST_NIL });
    }
    Location loc = { t.row, t.col, (unsigned int)(p->ptr - t.ptr) };
    return m.add({ loc, n1, n2, NN, NOEXTRA, NodeTy::EXPLIST_CONS });
  }

  unsigned int tyExp() {
    if (p->ty == KW_f32) return m.add({ tokenToLocation(p++), NN, NN, NN, NOEXTRA, NodeTy::f32 });
    if (p->ty == KW_f64) return m.add({ tokenToLocation(p++), NN, NN, NN, NOEXTRA, NodeTy::f64 });
    if (p->ty == KW_i8) return m.add({ tokenToLocation(p++), NN, NN, NN, NOEXTRA, NodeTy::i8 });
    if (p->ty == KW_i32) return m.add({ tokenToLocation(p++), NN, NN, NN, NOEXTRA, NodeTy::i32 });
    if (p->ty == KW_BOOL) return m.add({ tokenToLocation(p++), NN, NN, NN, NOEXTRA, NodeTy::BOOL });
    if (p->ty == KW_STRING) return m.add({ tokenToLocation(p++), NN, NN, NN, NOEXTRA, NodeTy::STRING });
    if (p->ty == KW_UNIT) return m.add({ tokenToLocation(p++), NN, NN, NN, NOEXTRA, NodeTy::UNIT });
    return EPSILON_ERROR;
  }

  unsigned int letStmt() {
    Token t = *p;
    if (p->ty == KW_LET) p++; else return EPSILON_ERROR;
    unsigned int n1 = ident();
    if (IS_ERROR(n1)) return ARRESTING_ERROR;
    if (p->ty == EQUAL) p++; else return ARRESTING_ERROR;
    unsigned int n2 = exp();
    if (IS_ERROR(n2)) return ARRESTING_ERROR;
    if (p->ty == SEMICOLON) p++; else return ARRESTING_ERROR;
    Location loc = { t.row, t.col, (unsigned int)(p->ptr - t.ptr) };
    return m.add({ loc, n1, n2, NN, NOEXTRA, NodeTy::LET });
  }

  unsigned int returnStmt() {
    Token t = *p;
    if (p->ty == KW_RETURN) p++; else return EPSILON_ERROR;
    unsigned n1 = exp();
    if (IS_ERROR(n1)) return ARRESTING_ERROR;
    if (p->ty == SEMICOLON) p++; else return ARRESTING_ERROR;
    Location loc = { t.row, t.col, (unsigned int)(p->ptr - t.ptr) };
    return m.add({ loc, n1, NN, NN, NOEXTRA, NodeTy::RETURN });
  }

  unsigned int stmt() {
    unsigned int n;
    n = letStmt();
    if (n != EPSILON_ERROR) return n;
    n = returnStmt();
    if (n != EPSILON_ERROR) return n;
    n = exp();
    if (IS_ERROR(n)) return n;
    if (p->ty == SEMICOLON) p++; else return ARRESTING_ERROR;
    return n;
  }

  /** Zero or more statements in a row. Never epsilon fails. */
  unsigned int stmts0() {
    Token t = *p;
    unsigned n1 = stmt();
    if (n1 == ARRESTING_ERROR) return ARRESTING_ERROR;
    if (n1 == EPSILON_ERROR) {
      Location loc = { t.row, t.col, 0 };
      return m.add({ loc, NN, NN, NN, NOEXTRA, NodeTy::STMTLIST_NIL });
    }
    unsigned n2 = stmts0();
    if (IS_ERROR(n2)) return ARRESTING_ERROR;
    Location loc = { t.row, t.col, (unsigned int)(p->ptr - t.ptr) };
    return m.add({ loc, n1, n2, NN, NOEXTRA, NodeTy::STMTLIST_CONS });
  }

  /** Parses parameter list of length zero or higher with optional terminal
   * comma (wotc). This parser never returns an epsilon error. */
  unsigned int paramListWotc0() {
    Token t = *p;
    unsigned int n1 = ident();
    if (n1 == ARRESTING_ERROR) return ARRESTING_ERROR;
    if (n1 == EPSILON_ERROR) {
      Location loc = { t.row, t.col, 0 };
      return m.add({ loc, NN, NN, NN, NOEXTRA, NodeTy::PARAMLIST_NIL });
    }
    if (p->ty == COLON) p++; else return ARRESTING_ERROR;
    unsigned int n2 = tyExp();
    if (IS_ERROR(n2)) return ARRESTING_ERROR;
    unsigned int n3;
    if (p->ty == COMMA) {
      p++;
      n3 = paramListWotc0();
    } else {
      Location loc1 = { p->row, p->col, 0 };
      n3 = m.add({ loc1, NN, NN, NN, NOEXTRA, NodeTy::PARAMLIST_NIL });
    }
    Location loc = { t.row, t.col, (unsigned int)(p->ptr - t.ptr) };
    return m.add({ loc, n1, n2, n3, NOEXTRA, NodeTy::PARAMLIST_CONS });
  }

  unsigned int funcOrProc() {
    Token t = *p;
    NodeTy ty;
    if (p->ty == KW_FUNC) { ty = NodeTy::FUNC; p++; }
    else if (p->ty == KW_PROC) { ty = NodeTy::PROC; p++; }
    else return EPSILON_ERROR;
    unsigned int n1 = ident();
    if (IS_ERROR(n1)) return ARRESTING_ERROR;
    if (p->ty == LPAREN) p++; else return EPSILON_ERROR;
    unsigned int n2 = paramListWotc0();
    if (IS_ERROR(n2)) return ARRESTING_ERROR;
    if (p->ty == RPAREN) p++; else return ARRESTING_ERROR;
    if (p->ty == COLON) p++; else return ARRESTING_ERROR;
    unsigned int n3 = tyExp();
    if (IS_ERROR(n3)) return ARRESTING_ERROR;
    if (p->ty == EQUAL) p++; else return ARRESTING_ERROR;
    unsigned int n4 = exp();
    if (IS_ERROR(n4)) return ARRESTING_ERROR;
    if (p->ty == SEMICOLON) p++; else return ARRESTING_ERROR;
    Location loc = { t.row, t.col, (unsigned int)(p->ptr - t.ptr) };
    return m.add({ loc, n1, n2, n3, { .nodes={ n4, NN } }, ty });
  }

  bool hasMoreToParse() {
    return p < end && p->ty != END;
  }

  Location tokenToLocation(std::vector<Token>::iterator tok) {
    return { tok->row, tok->col, tok->sz };
  }

  /** Creates a new `TYPEVAR` node for the typer to fill in later. */
  unsigned int newTYPEVAR() {
    return m.add({ { 0, 0, 0 }, NN, NN, NN, { .intVal = nextUnusedTypeVarID++ }, NodeTy::TYPEVAR });
  }

};

#endif