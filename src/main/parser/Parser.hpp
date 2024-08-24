#ifndef PARSER_PARSER
#define PARSER_PARSER

#include <cstdio>
#include <vector>
#include "common/NodeManager.hpp"

#define ARRESTING_ERROR 0xffffffff
#define EPSILON_ERROR   0xfffffffe
#define IS_ERROR(x)     ((x) >= 0xfffffffe)

/** Placed in sub-node slots in a node to indicate "no node" */
#define NN              0xffffffff

class Parser {

public:
  std::vector<Token> tokens;
  std::vector<Token>::iterator p;
  std::vector<Token>::iterator end;
  NodeManager m;

  Parser(std::vector<Token> &_tokens) {
    tokens = _tokens;
    p = tokens.begin();
    end = tokens.end();
  }

  unsigned int numLit() {
    NodeTy ty;
    if (p->ty == LIT_INT) ty = NodeTy::LIT_INT;
    else if (p->ty == LIT_DEC) ty = NodeTy::LIT_DEC;
    else return EPSILON_ERROR;
    return m.add({ tokenToLocation(p++), NN, NN, NN, ty });
  }

  unsigned int litString() {
    if (p->ty == LIT_STRING) return m.add({ tokenToLocation(p++), NN, NN, NN, NodeTy::LIT_STRING });
    else return EPSILON_ERROR;
  }

  unsigned int ident() {
    if (p->ty == TOK_IDENT) return m.add({ tokenToLocation(p++), NN, NN, NN, NodeTy::IDENT });
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
    Location loc = tokenToLocation(p);
    if (p->ty == LBRACE) p++; else return EPSILON_ERROR;
    unsigned int n = stmts0();
    if (IS_ERROR(n)) return ARRESTING_ERROR;
    if (p->ty == RBRACE) p++; else return EPSILON_ERROR;
    loc.sz = (unsigned int)((const char*)p.base() - loc.ptr);
    return m.add({ loc, n, NN, NN, NodeTy::BLOCK });
  }

  unsigned int identOrFunctionCall() {
    unsigned int n1 = ident();
    if (IS_ERROR(n1)) return n1;
    if (p->ty == LPAREN) {
      p++;
      unsigned int n2 = expListWotc0();
      if (p->ty == RPAREN) p++; else return ARRESTING_ERROR;
      Location loc = m.get(n1).loc;
      loc.sz = (unsigned int)((const char*)p.base() - loc.ptr);
      return m.add({ loc, n1, n2, NN, NodeTy::CALL });
    } else {
      return n1;
    }
  }

  unsigned int expLv0() {
    unsigned int n1;
    n1 = identOrFunctionCall();
    if (n1 != EPSILON_ERROR) return n1;
    n1 = numLit();
    if (n1 != EPSILON_ERROR) return n1;
    n1 = litString();
    if (n1 != EPSILON_ERROR) return n1;
    n1 = parensExp();
    if (n1 != EPSILON_ERROR) return n1;
    return blockExp();
  }

  unsigned int expLv1() {
    Location loc = tokenToLocation(p);
    unsigned int e = expLv0();
    if (IS_ERROR(e)) return e;
    while (p->ty == OP_MUL || p->ty == OP_DIV) {
      NodeTy op = (p->ty == OP_MUL) ? NodeTy::MUL : NodeTy::DIV;
      p++;
      unsigned int e2 = expLv0();
      if (IS_ERROR(e2)) return ARRESTING_ERROR;
      loc.sz = (unsigned int)((const char*)p.base() - loc.ptr);
      e = m.add({ loc, e, e2, NN, op });
    }
    return e;
  }

  unsigned int expLv2() {
    Location loc = tokenToLocation(p);
    unsigned int e = expLv1();
    if (IS_ERROR(e)) return e;
    while (p->ty == OP_ADD || p->ty == OP_SUB) {
      NodeTy op = (p->ty == OP_ADD) ? NodeTy::ADD : NodeTy::SUB;
      p++;
      unsigned int e2 = expLv1();
      if (IS_ERROR(e2)) return ARRESTING_ERROR;
      loc.sz = (unsigned int)((const char*)p.base() - loc.ptr);
      e = m.add({ loc, e, e2, NN, op });
    }
    return e;
  }

  unsigned int expLv3() {
    Location loc = tokenToLocation(p);
    unsigned int e = expLv2();
    if (IS_ERROR(e)) return e;
    while (p->ty == OP_EQ || p->ty == OP_NEQ) {
      NodeTy op = (p->ty == OP_EQ) ? NodeTy::EQ : NodeTy::NE;
      p++;
      unsigned int e2 = expLv2();
      if (IS_ERROR(e2)) return ARRESTING_ERROR;
      loc.sz = (unsigned int)((const char*)p.base() - loc.ptr);
      e = m.add({ loc, e, e2, NN, op });
    }
    return e;
  }

  unsigned int ifExp() {
    Location loc = tokenToLocation(p);
    if (p->ty == KW_IF) p++; else return EPSILON_ERROR;
    unsigned int n1 = exp();
    if (IS_ERROR(n1)) return ARRESTING_ERROR;
    if (p->ty == KW_THEN) p++; else return ARRESTING_ERROR;
    unsigned int n2 = exp();
    if (IS_ERROR(n2)) return ARRESTING_ERROR;
    if (p->ty == KW_ELSE) p++; else return ARRESTING_ERROR;
    unsigned int n3 = exp();
    if (IS_ERROR(n3)) return ARRESTING_ERROR;
    loc.sz = (unsigned int)((const char*)p.base() - loc.ptr);
    return m.add({ loc, n1, n2, n3, NodeTy::IF });
  }

  unsigned int exp() {
    unsigned int n = ifExp();
    if (n != EPSILON_ERROR) return n;
    return expLv3();
  }

  /** Zero or more comma-separated expressions with optional trailing
   * comma (wotc). Never epsilon fails. */
  unsigned int expListWotc0() {
    Location loc = tokenToLocation(p);
    unsigned int n1 = exp();
    if (n1 == ARRESTING_ERROR) return ARRESTING_ERROR;
    if (n1 == EPSILON_ERROR) {
      loc.sz = 0;
      return m.add({ loc, NN, NN, NN, NodeTy::EXPLIST_NIL });
    }
    unsigned int n2;
    if (p->ty == COMMA) {
      p++;
      n2 = expListWotc0();
    } else {
      n2 = m.add({ tokenToLocation(p), NN, NN, NN, NodeTy::EXPLIST_NIL });
    }
    loc.sz = (unsigned int)((const char*)p.base() - loc.ptr);
    return m.add({ loc, n1, n2, NN, NodeTy::EXPLIST_CONS });
  }

  unsigned int tyExp() {
    if (p->ty == KW_i8) return m.add({ tokenToLocation(p++), NN, NN, NN, NodeTy::i8 });
    if (p->ty == KW_i32) return m.add({ tokenToLocation(p++), NN, NN, NN, NodeTy::i32 });
    if (p->ty == KW_BOOL) return m.add({ tokenToLocation(p++), NN, NN, NN, NodeTy::BOOL });
    return EPSILON_ERROR;
  }

  unsigned int letStmt() {
    Location loc = tokenToLocation(p);
    if (p->ty == KW_LET) p++; else return EPSILON_ERROR;
    unsigned int n1 = ident();
    if (IS_ERROR(n1)) return ARRESTING_ERROR;
    if (p->ty == EQUAL) p++; else return ARRESTING_ERROR;
    unsigned int n2 = exp();
    if (IS_ERROR(n2)) return ARRESTING_ERROR;
    if (p->ty == SEMICOLON) p++; else return ARRESTING_ERROR;
    loc.sz = (unsigned int)((const char*)p.base() - loc.ptr);
    return m.add({ loc, n1, n2, NN, NodeTy::LET });
  }

  unsigned int returnStmt() {
    Location loc = tokenToLocation(p);
    if (p->ty == KW_RETURN) p++; else return EPSILON_ERROR;
    unsigned n1 = exp();
    if (IS_ERROR(n1)) return ARRESTING_ERROR;
    if (p->ty == SEMICOLON) p++; else return ARRESTING_ERROR;
    loc.sz = (unsigned int)((const char*)p.base() - loc.ptr);
    return m.add({ loc, n1, NN, NN, NodeTy::RETURN });
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
    Location loc = tokenToLocation(p);
    unsigned n1 = stmt();
    if (n1 == ARRESTING_ERROR) return ARRESTING_ERROR;
    if (n1 == EPSILON_ERROR) {
      loc.sz = 0;
      return m.add({ loc, NN, NN, NN, NodeTy::STMTLIST_NIL });
    }
    unsigned n2 = stmts0();
    if (IS_ERROR(n2)) return ARRESTING_ERROR;
    loc.sz = (unsigned int)((const char*)p.base() - loc.ptr);
    return m.add({ loc, n1, n2, NN, NodeTy::STMTLIST_CONS });
  }

  /** Parses parameter list of length zero or higher with optional terminal
   * comma (wotc). This parser never returns an epsilon error. */
  unsigned int paramListWotc0() {
    Location loc = tokenToLocation(p);
    unsigned int n1 = ident();
    if (n1 == ARRESTING_ERROR) return ARRESTING_ERROR;
    if (n1 == EPSILON_ERROR) {
      loc.sz = 0;
      return m.add({ loc, NN, NN, NN, NodeTy::PARAMLIST_NIL });
    }
    if (p->ty == COLON) p++; else return ARRESTING_ERROR;
    unsigned int n2 = tyExp();
    if (IS_ERROR(n2)) return ARRESTING_ERROR;
    unsigned int n3;
    if (p->ty == COMMA) {
      p++;
      n3 = paramListWotc0();
    } else {
      n3 = m.add({ tokenToLocation(p), NN, NN, NN, NodeTy::PARAMLIST_NIL });
    }
    loc.sz = (unsigned int)((const char*)p.base() - loc.ptr);
    return m.add({ loc, n1, n2, n3, NodeTy::PARAMLIST_CONS });
  }

  /** A signature is the parameter list and return type annotation of a
   * function decl. */
  unsigned int signature() {
    Location loc = tokenToLocation(p);
    if (p->ty == LPAREN) p++; else return EPSILON_ERROR;
    unsigned int n1 = paramListWotc0();
    if (IS_ERROR(n1)) return ARRESTING_ERROR;
    if (p->ty == RPAREN) p++; else return ARRESTING_ERROR;
    if (p->ty == COLON) p++; else return ARRESTING_ERROR;
    unsigned int n2 = tyExp();
    if (IS_ERROR(n2)) return ARRESTING_ERROR;
    loc.sz = (unsigned int)((const char*)p.base() - loc.ptr);
    return m.add({ loc, n1, n2, NN, NodeTy::SIGNATURE });
  }

  unsigned int funcOrProc() {
    NodeTy ty;
    if (p->ty == KW_FUNC) { ty = NodeTy::FUNC; p++; }
    else if (p->ty == KW_PROC) { ty = NodeTy::PROC; p++; }
    else return EPSILON_ERROR;
    unsigned int n1 = ident();
    if (IS_ERROR(n1)) return ARRESTING_ERROR;
    unsigned int n2 = signature();
    if (IS_ERROR(n2)) return ARRESTING_ERROR;
    if (p->ty == EQUAL) p++; else return ARRESTING_ERROR;
    unsigned int n3 = exp();
    if (IS_ERROR(n3)) return ARRESTING_ERROR;
    if (p->ty == SEMICOLON) p++; else return ARRESTING_ERROR;
    Location loc = m.get(n1).loc;
    loc.sz = (unsigned int)((const char*)p.base() - loc.ptr);
    return m.add({ loc, n1, n2, n3, NodeTy::FUNC });
  }

  bool hasMoreToParse() {
    return p < end && p->ty != END;
  }

  Location tokenToLocation(std::vector<Token>::iterator tok) {
    return { tok->ptr, tok->sz, tok->row, tok->col };
  }

};

#endif