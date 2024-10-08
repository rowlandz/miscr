#ifndef LEXER_LEXER
#define LEXER_LEXER

#include "common/LocatedError.hpp"
#include "lexer/Scanner.hpp"
#include <cstring>
#include <vector>

enum LexerST: unsigned char {
  ST_BEGIN,
  ST_DIGITS,
  ST_DIGITS_DOT_DIGITS,
  ST_IDENT,
  ST_EQUAL,
  ST_RANGLE,
  ST_LANGLE,
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

/// @brief The lexer takes a `const char* text` string and generates a vector
/// of tokens which is fed to the parser.
///
/// The `run` method runs the lexer and returns a success indicator. If
/// successful, `getTokens` and `getLocationTable` can be used to retrieve data.
/// Otherwise, `getError` will return an error message.
class Lexer {
  Scanner<LexerST> tok;
  LocatedError err;

public:
  Lexer(llvm::StringRef text) : tok(text, ST_BEGIN), err('^') {}

  /// Runs the lexer. Returns `true` if tokenization was successful.
  bool run() {
    while (tok.thereAreMoreChars()) {
      if (!one_iteration()) return false;
    }
    final_iteration();
    tok.capture(Token::END);
    return true;
  }

  /// Returns the lexed tokens after a successful run.
  const std::vector<Token>& getTokens() { return tok.tokens(); }

  /// Returns the location table after a successful run.
  const LocationTable& getLocationTable() { return tok.locationTable(); }

  /// Returns the lexer error after a failed run. */
  LocatedError getError() { return err; }

private:

  // TODO: alphabetize cases
  bool one_iteration() {
    char c = tok.currentChar();
    switch (tok.state()) {
      case ST_BEGIN:
        if (isWhitespace(c)) tok.stepAndDiscard();
        else if (isDigit(c)) tok.step(ST_DIGITS);
        else if (isAlpha(c) || c == '_') tok.step(ST_IDENT);
        else if (c == '"') tok.step(ST_STRING);
        else if (c == '/') tok.step(ST_FSLASH);
        else if (c == '=') tok.step(ST_EQUAL);
        else if (c == '>') tok.step(ST_RANGLE);
        else if (c == '<') tok.step(ST_LANGLE);
        else if (c == ':') tok.step(ST_COLON);
        else if (c == '&') tok.stepAndCapture(Token::AMP);
        else if (c == '#') tok.stepAndCapture(Token::HASH);
        else if (c == '!') tok.stepAndCapture(Token::EXCLAIM);
        else if (c == '+') tok.stepAndCapture(Token::OP_ADD);
        else if (c == '-') tok.stepAndCapture(Token::OP_SUB);
        else if (c == '*') tok.stepAndCapture(Token::OP_MUL);
        else if (c == '(') tok.stepAndCapture(Token::LPAREN);
        else if (c == ')') tok.stepAndCapture(Token::RPAREN);
        else if (c == '{') tok.stepAndCapture(Token::LBRACE);
        else if (c == '}') tok.stepAndCapture(Token::RBRACE);
        else if (c == '[') tok.stepAndCapture(Token::LBRACKET);
        else if (c == ']') tok.stepAndCapture(Token::RBRACKET);
        else if (c == ',') tok.stepAndCapture(Token::COMMA);
        else if (c == '.') tok.stepAndCapture(Token::DOT);
        else if (c == ';') tok.stepAndCapture(Token::SEMICOLON);
        else {
          err.append("Illegal start of token.\n");
          err.append(tok.currentLocation());
          return false;
        }
        break;

      case ST_IDENT:
        if (isAlphaNumU(c)) tok.step(ST_IDENT);
        else tok.capture(identOrKeywordTy(tok.selectionPtr(), tok.selectionSize()));
        break;

      case ST_DIGITS:
        if (isDigit(c)) tok.step(ST_DIGITS);
        else if (c == '.') tok.step(ST_DIGITS_DOT_DIGITS); 
        else tok.capture(Token::LIT_INT);
        break;
      
      case ST_DIGITS_DOT_DIGITS:
        if (isDigit(c)) tok.step(ST_DIGITS_DOT_DIGITS);
        else tok.capture(Token::LIT_DEC);
        break;

      case ST_EQUAL:
        if (c == '>') tok.stepAndCapture(Token::FATARROW);
        else if (c == '=') tok.stepAndCapture(Token::OP_EQ);
        else tok.capture(Token::EQUAL);
        break;

      case ST_LANGLE:
        if (c == '=') tok.stepAndCapture(Token::OP_LE);
        else tok.capture(Token::OP_LT);
        break;

      case ST_RANGLE:
        if (c == '=') tok.stepAndCapture(Token::OP_GE);
        else tok.capture(Token::OP_GT);
        break;

      case ST_COLON:
        if (c == ':') tok.stepAndCapture(Token::COLON_COLON);
        else if (c == '=') tok.stepAndCapture(Token::COLON_EQUAL);
        else tok.capture(Token::COLON);
        break;

      case ST_STRING:
        if (c == '"') tok.stepAndCapture(Token::LIT_STRING);
        else if (c == '\\') tok.step(ST_STRING_BSLASH);
        else if (c == '\n') { tok.capture(Token::LIT_STRING); tok.stepAndDiscard(); }
        else tok.step(ST_STRING);
        break;

      case ST_STRING_BSLASH:
        tok.step(ST_STRING);
        break;

      case ST_FSLASH:
        if (c == '/') tok.step(ST_FSLASH_FSLASH);
        else if (c == '*') tok.step(ST_FSLASH_STAR);
        else if (c == '=') tok.stepAndCapture(Token::OP_NE);
        else tok.capture(Token::OP_DIV);
        break;

      case ST_FSLASH_STAR:
        if (c == '*') tok.step(ST_FSLASH_STAR_STAR);
        else tok.step(ST_MULTILINE_COMMENT);
        break;

      case ST_FSLASH_STAR_STAR:
        if (c == '/') tok.stepAndDiscard();   // discard non-doc comments
        else tok.step(ST_MULTILINE_DOC_COMMENT);
        break;

      case ST_FSLASH_FSLASH:
        if (c == '<') tok.step(ST_LINE_COMMENT_L);
        else if (c == '>') tok.step(ST_LINE_COMMENT_R);
        else if (c == '\n') tok.stepAndDiscard();   // discard non-doc comments
        else tok.step(ST_LINE_COMMENT);
        break;

      case ST_LINE_COMMENT:
        if (c == '\n') tok.stepAndDiscard();   // discard non-doc comments
        else tok.step(ST_LINE_COMMENT);
        break;

      case ST_LINE_COMMENT_L:
        if (c == '\n') { tok.capture(Token::DOC_COMMENT_L); tok.stepAndDiscard(); }
        else tok.step(ST_LINE_COMMENT_L);
        break;

      case ST_LINE_COMMENT_R:
        if (c == '\n') { tok.capture(Token::DOC_COMMENT_R); tok.stepAndDiscard(); }
        else tok.step(ST_LINE_COMMENT_R);
        break;

      case ST_MULTILINE_COMMENT:
        if (c == '*') tok.step(ST_MULTILINE_COMMENT_STAR);
        else tok.step(ST_MULTILINE_COMMENT);
        break;

      case ST_MULTILINE_COMMENT_STAR:
        if (c == '/') tok.stepAndDiscard();   // discard non-doc comments
        else if (c == '*') tok.step(ST_MULTILINE_COMMENT_STAR);
        else tok.step(ST_MULTILINE_COMMENT);
        break;

      case ST_MULTILINE_DOC_COMMENT:
        if (c == '*') tok.step(ST_MULTILINE_DOC_COMMENT_STAR);
        else tok.step(ST_MULTILINE_DOC_COMMENT);
        break;

      case ST_MULTILINE_DOC_COMMENT_STAR:
        if (c == '/') tok.stepAndCapture(Token::DOC_COMMENT_R);
        else if (c == '*') tok.step(ST_MULTILINE_DOC_COMMENT_STAR);
        else tok.step(ST_MULTILINE_DOC_COMMENT);
        break;
    }

    return true;
  }

  void final_iteration() {
    switch (tok.state()) {
      case ST_BEGIN:
        break;
      case ST_DIGITS:
        tok.capture(Token::LIT_INT);
        break;
      case ST_DIGITS_DOT_DIGITS:
        tok.capture(Token::LIT_DEC);
        break;
      case ST_IDENT:
        tok.capture(identOrKeywordTy(tok.selectionPtr(), tok.selectionSize()));
        break;
      case ST_EQUAL:
        tok.capture(Token::EQUAL);
        break;
      case ST_LANGLE:
        tok.capture(Token::OP_LT);
        break;
      case ST_RANGLE:
        tok.capture(Token::OP_GT);
        break;
      case ST_COLON:
        tok.capture(Token::COLON);
        break;
      case ST_STRING:
      case ST_STRING_BSLASH:
        tok.capture(Token::LIT_STRING);
        break;
      case ST_FSLASH:
        tok.capture(Token::OP_DIV);
        break;
      case ST_FSLASH_FSLASH:
      case ST_FSLASH_STAR:
      case ST_FSLASH_STAR_STAR:
      case ST_LINE_COMMENT:
      case ST_MULTILINE_COMMENT:
      case ST_MULTILINE_COMMENT_STAR:
        // discard non-doc comments
        break;
      case ST_LINE_COMMENT_L:
        tok.capture(Token::DOC_COMMENT_L);
        break;
      case ST_LINE_COMMENT_R:
      case ST_MULTILINE_DOC_COMMENT:
      case ST_MULTILINE_DOC_COMMENT_STAR:
        tok.capture(Token::DOC_COMMENT_R);
        break;
    }
  }

  /** If `s` is a keyword, returns the corresponding token tag,
   * otherwise returns TOK_IDENT. */
  Token::Tag identOrKeywordTy(const char* s, int len) {
    switch (len) {
      case 1:
        if (!strncmp("_", s, 1)) return Token::UNDERSCORE;
      case 2:
        if (!strncmp("i8", s, 2)) return Token::KW_i8;
        if (!strncmp("if", s, 2)) return Token::KW_IF;
        if (!strncmp("of", s, 2)) return Token::KW_OF;
        return Token::TOK_IDENT;
      case 3:
        if (!strncmp("f32", s, 3)) return Token::KW_f32;
        if (!strncmp("f64", s, 3)) return Token::KW_f64;
        if (!strncmp("i16", s, 3)) return Token::KW_i16;
        if (!strncmp("i32", s, 3)) return Token::KW_i32;
        if (!strncmp("i64", s, 3)) return Token::KW_i64;
        if (!strncmp("let", s, 3)) return Token::KW_LET;
        if (!strncmp("str", s, 3)) return Token::KW_STR;
        return Token::TOK_IDENT;
      case 4:
        if (!strncmp("bool", s, 4)) return Token::KW_BOOL;
        if (!strncmp("case", s, 4)) return Token::KW_CASE;
        if (!strncmp("data", s, 4)) return Token::KW_DATA;
        if (!strncmp("else", s, 4)) return Token::KW_ELSE;
        if (!strncmp("func", s, 4)) return Token::KW_FUNC;
        if (!strncmp("proc", s, 4)) return Token::KW_PROC;
        if (!strncmp("then", s, 4)) return Token::KW_THEN;
        if (!strncmp("true", s, 4)) return Token::KW_TRUE;
        if (!strncmp("unit", s, 4)) return Token::KW_UNIT;
        return Token::TOK_IDENT;
      case 5:
        if (!strncmp("false", s, 5)) return Token::KW_FALSE;
        if (!strncmp("match", s, 5)) return Token::KW_MATCH;
        return Token::TOK_IDENT;
      case 6:
        if (!strncmp("extern", s, 6)) return Token::KW_EXTERN;
        if (!strncmp("module", s, 6)) return Token::KW_MODULE;
        if (!strncmp("return", s, 6)) return Token::KW_RETURN;
        return Token::TOK_IDENT;
      case 9:
        if (!strncmp("namespace", s, 9)) return Token::KW_NAMESPACE;
        return Token::TOK_IDENT;
      default:
        return Token::TOK_IDENT;
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
