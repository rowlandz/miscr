#ifndef LEXER_LEXER
#define LEXER_LEXER

#include "lexer/Scanner.hpp"
#include <cstring>
#include <vector>

enum LexerST: unsigned char {
  ST_BEGIN,
  ST_DIGITS,
  ST_DIGITS_DOT_DIGITS,
  ST_IDENT,
  ST_EQUAL,
  ST_COLON,
  ST_STRING,
  ST_STRING_BSLASH,
  ST_FSLASH,
  ST_FSLASH_STAR,
  ST_FSLASH_STAR_STAR,
  ST_FSLASH_FSLASH,
  ST_LINE_COMMENT,
  ST_LINE_COMMENT_L,
  ST_LINE_COMMENT_R,
  ST_MULTILINE_COMMENT,
  ST_MULTILINE_COMMENT_STAR,
  ST_MULTILINE_DOC_COMMENT,
  ST_MULTILINE_DOC_COMMENT_STAR,
};

/** The lexer. */
class Lexer {

public:
  std::vector<Token> run(const char* text) {
    tok = Scanner<LexerST>(text, ST_BEGIN);

    while (tok.thereAreMoreChars()) {
      one_iteration();
    }
    final_iteration();
    tok.capture(END);

    return tok.tokens();
  }

private:
  Scanner<LexerST> tok;

  inline void one_iteration() {
    char c = tok.currentChar();
    switch (tok.state()) {
      case ST_BEGIN:
        if (isWhitespace(c)) tok.stepAndDiscard();
        else if (isDigit(c)) tok.step(ST_DIGITS);
        else if (isAlpha(c) || c == '_') tok.step(ST_IDENT);
        else if (c == '"') tok.step(ST_STRING);
        else if (c == '/') tok.step(ST_FSLASH);
        else if (c == '=') tok.step(ST_EQUAL);
        else if (c == ':') tok.step(ST_COLON);
        else if (c == '+') tok.stepAndCapture(OP_ADD);
        else if (c == '-') tok.stepAndCapture(OP_SUB);
        else if (c == '*') tok.stepAndCapture(OP_MUL);
        else if (c == '(') tok.stepAndCapture(LPAREN);
        else if (c == ')') tok.stepAndCapture(RPAREN);
        else if (c == '{') tok.stepAndCapture(LBRACE);
        else if (c == '}') tok.stepAndCapture(RBRACE);
        else if (c == ',') tok.stepAndCapture(COMMA);
        else if (c == ';') tok.stepAndCapture(SEMICOLON);
        else tok.stepAndDiscard();
        break;

      case ST_IDENT:
        if (isAlphaNumU(c)) tok.step(ST_IDENT);
        else tok.capture(identOrKeywordTy(tok.selectionPtr(), tok.selectionSize()));
        break;

      case ST_DIGITS:
        if (isDigit(c)) tok.step(ST_DIGITS);
        else if (c == '.') tok.step(ST_DIGITS_DOT_DIGITS); 
        else tok.capture(LIT_INT);
        break;
      
      case ST_DIGITS_DOT_DIGITS:
        if (isDigit(c)) tok.step(ST_DIGITS_DOT_DIGITS);
        else tok.capture(LIT_DEC);
        break;

      case ST_EQUAL:
        if (c == '>') tok.stepAndCapture(FATARROW);
        else if (c == '=') tok.stepAndCapture(OP_EQ);
        else tok.capture(EQUAL);
        break;

      case ST_COLON:
        if (c == ':') tok.stepAndCapture(COLON_COLON);
        else tok.capture(COLON);
        break;

      case ST_STRING:
        if (c == '"') tok.stepAndCapture(LIT_STRING);
        else if (c == '\\') tok.step(ST_STRING_BSLASH);
        else if (c == '\n') { tok.capture(LIT_STRING); tok.stepAndDiscard(); }
        else tok.step(ST_STRING);
        break;

      case ST_STRING_BSLASH:
        tok.step(ST_STRING);
        break;

      case ST_FSLASH:
        if (c == '/') tok.step(ST_FSLASH_FSLASH);
        else if (c == '*') tok.step(ST_FSLASH_STAR);
        else if (c == '=') tok.stepAndCapture(OP_NEQ);
        else tok.capture(OP_DIV);
        break;

      case ST_FSLASH_STAR:
        if (c == '*') tok.step(ST_FSLASH_STAR_STAR);
        else tok.step(ST_MULTILINE_COMMENT);
        break;

      case ST_FSLASH_STAR_STAR:
        if (c == '/') tok.stepAndCapture(COMMENT);
        else tok.step(ST_MULTILINE_DOC_COMMENT);
        break;

      case ST_FSLASH_FSLASH:
        if (c == '<') tok.step(ST_LINE_COMMENT_L);
        else if (c == '>') tok.step(ST_LINE_COMMENT_R);
        else if (c == '\n') { tok.capture(COMMENT); tok.stepAndDiscard(); }
        else tok.step(ST_LINE_COMMENT);
        break;

      case ST_LINE_COMMENT:
        if (c == '\n') { tok.capture(COMMENT); tok.stepAndDiscard(); }
        else tok.step(ST_LINE_COMMENT);
        break;

      case ST_LINE_COMMENT_L:
        if (c == '\n') { tok.capture(DOC_COMMENT_L); tok.stepAndDiscard(); }
        else tok.step(ST_LINE_COMMENT_L);
        break;

      case ST_LINE_COMMENT_R:
        if (c == '\n') { tok.capture(DOC_COMMENT_R); tok.stepAndDiscard(); }
        else tok.step(ST_LINE_COMMENT_R);
        break;

      case ST_MULTILINE_COMMENT:
        if (c == '*') tok.step(ST_MULTILINE_COMMENT_STAR);
        else tok.step(ST_MULTILINE_COMMENT);
        break;

      case ST_MULTILINE_COMMENT_STAR:
        if (c == '/') tok.stepAndCapture(COMMENT);
        else if (c == '*') tok.step(ST_MULTILINE_COMMENT_STAR);
        else tok.step(ST_MULTILINE_COMMENT);
        break;

      case ST_MULTILINE_DOC_COMMENT:
        if (c == '*') tok.step(ST_MULTILINE_DOC_COMMENT_STAR);
        else tok.step(ST_MULTILINE_DOC_COMMENT);
        break;

      case ST_MULTILINE_DOC_COMMENT_STAR:
        if (c == '/') tok.stepAndCapture(DOC_COMMENT_R);
        else if (c == '*') tok.step(ST_MULTILINE_DOC_COMMENT_STAR);
        else tok.step(ST_MULTILINE_DOC_COMMENT);
        break;
    }
  }

  inline void final_iteration() {
    switch (tok.state()) {
      case ST_BEGIN:
        break;
      case ST_DIGITS:
        tok.capture(LIT_INT);
        break;
      case ST_DIGITS_DOT_DIGITS:
        tok.capture(LIT_DEC);
        break;
      case ST_IDENT:
        tok.capture(identOrKeywordTy(tok.selectionPtr(), tok.selectionSize()));
        break;
      case ST_EQUAL:
        tok.capture(EQUAL);
        break;
      case ST_COLON:
        tok.capture(COLON);
        break;
      case ST_STRING:
      case ST_STRING_BSLASH:
        tok.capture(LIT_STRING);
        break;
      case ST_FSLASH:
        tok.capture(OP_DIV);
        break;
      case ST_FSLASH_FSLASH:
      case ST_FSLASH_STAR:
      case ST_FSLASH_STAR_STAR:
      case ST_LINE_COMMENT:
      case ST_MULTILINE_COMMENT:
      case ST_MULTILINE_COMMENT_STAR:
        tok.capture(COMMENT);
        break;
      case ST_LINE_COMMENT_L:
        tok.capture(DOC_COMMENT_L);
        break;
      case ST_LINE_COMMENT_R:
      case ST_MULTILINE_DOC_COMMENT:
      case ST_MULTILINE_DOC_COMMENT_STAR:
        tok.capture(DOC_COMMENT_R);
        break;
    }
  }

  /** If `s` is a keyword, returns the corresponding token type,
   * otherwise returns TOK_IDENT. */
  TokenTy identOrKeywordTy(const char* s, int len) {
    switch (len) {
      case 2:
        if (!strncmp("i8", s, 2)) return KW_i8;
        if (!strncmp("if", s, 2)) return KW_IF;
        return TOK_IDENT;
      case 3:
        if (!strncmp("f32", s, 3)) return KW_f32;
        if (!strncmp("f64", s, 3)) return KW_f64;
        if (!strncmp("i32", s, 3)) return KW_i32;
        if (!strncmp("let", s, 3)) return KW_LET;
        return TOK_IDENT;
      case 4:
        if (!strncmp("bool", s, 4)) return KW_BOOL;
        if (!strncmp("case", s, 4)) return KW_CASE;
        if (!strncmp("else", s, 4)) return KW_ELSE;
        if (!strncmp("func", s, 4)) return KW_FUNC;
        if (!strncmp("proc", s, 4)) return KW_PROC;
        if (!strncmp("then", s, 4)) return KW_THEN;
        if (!strncmp("true", s, 4)) return KW_TRUE;
        if (!strncmp("unit", s, 4)) return KW_UNIT;
        return TOK_IDENT;
      case 5:
        if (!strncmp("false", s, 5)) return KW_FALSE;
        if (!strncmp("match", s, 5)) return KW_MATCH;
        return TOK_IDENT;
      case 6:
        if (!strncmp("extern", s, 6)) return KW_EXTERN;
        if (!strncmp("module", s, 6)) return KW_MODULE;
        if (!strncmp("return", s, 6)) return KW_RETURN;
        if (!strncmp("string", s, 6)) return KW_STRING;
        return TOK_IDENT;
      case 9:
        if (!strncmp("namespace", s, 9)) return KW_NAMESPACE;
        return TOK_IDENT;
      default:
        return TOK_IDENT;
    }
  }

  bool isDigit(char c) {
    return '0' <= c && c <= '9';
  }

  bool isAlpha(char c) {
    return ('A' <= c && c <= 'Z')
        || ('a' <= c && c <= 'z');
  }

  bool isAlphaNumU(char c) {
    return ('A' <= c && c <= 'Z')
        || ('a' <= c && c <= 'z')
        || ('0' <= c && c <= '9')
        || c == '_';
  }

  bool isWhitespace(char c) {
    return c == ' ' || c == '\n' || c == '\t';
  }
};

#endif
