#ifndef LEXER_LEXER
#define LEXER_LEXER

#include "common/LocatedError.hpp"
#include "lexer/Scanner.hpp"
#include <vector>

/// @brief The MiSCR lexer. Converts a null-terminated string into a Token
/// vector that is passed to the Parser.
///
/// The run() method runs the lexer and returns a success indicator. If
/// successful, getTokens() and getLocationTable() can be used to retrieve data.
/// Otherwise, getError() will return an error message.
class Lexer {
public:

  /// @brief Builds a lexer that will scan @p text.
  Lexer(llvm::StringRef text) : tok(text, ST::BEGIN), err('^') {}

  /// @brief Runs the lexer. Returns `true` iff tokenization was successful.
  bool run() {
    while (tok.thereAreMoreChars()) {
      if (!oneIteration()) return false;
    }
    if (!finalIteration()) return false;
    tok.capture(Token::END);
    return true;
  }

  /// @brief Returns the lexed tokens after a successful run.
  const std::vector<Token>& getTokens() { return tok.tokens(); }

  /// @brief Returns the location table after a successful run.
  const LocationTable& getLocationTable() { return tok.locationTable(); }

  /// @brief Returns the lexer error after a failed run.
  LocatedError getError() { return err; }

private:

  /// @brief The state of the Lexer state machine.
  enum class ST: unsigned char {
    ANGLE_L,
    ANGLE_R,
    BEGIN,
    COLON,
    DIGITS,
    DIGITS_DOT_DIGITS,
    EQUAL,
    FSLASH,
    FSLASH_FSLASH,
    FSLASH_STAR,
    FSLASH_STAR_STAR,
    IDENT,
    LINE_COMMENT,
    LINE_COMMENT_L,
    LINE_COMMENT_R,
    MULTILINE_COMMENT,
    MULTILINE_COMMENT_STAR,
    MULTILINE_DOC_COMMENT,
    MULTILINE_DOC_COMMENT_STAR,
    STRING,
    STRING_BSLASH,
  };

  Scanner<ST> tok;
  LocatedError err;

  /// @brief Either scans a single char or scans no chars and resets the state
  /// to ST::BEGIN.
  bool oneIteration() {
    char c = tok.currentChar();
    switch (tok.state()) {
    
    case ST::ANGLE_L:
      if (c == '=') tok.stepAndCapture(Token::OP_LE);
      else tok.capture(Token::OP_LT);
      break;

    case ST::ANGLE_R:
      if (c == '=') tok.stepAndCapture(Token::OP_GE);
      else tok.capture(Token::OP_GT);
      break;

    case ST::BEGIN:
      if (isWhitespace(c)) tok.stepAndDiscard();
      else if (isDigit(c)) tok.step(ST::DIGITS);
      else if (isAlpha(c) || c == '_') tok.step(ST::IDENT);
      else if (c == '"') tok.step(ST::STRING);
      else if (c == '/') tok.step(ST::FSLASH);
      else if (c == '=') tok.step(ST::EQUAL);
      else if (c == '>') tok.step(ST::ANGLE_R);
      else if (c == '<') tok.step(ST::ANGLE_L);
      else if (c == ':') tok.step(ST::COLON);
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
        err.append(tok.selectionBeginLocation());
        return false;
      }
      break;

    case ST::COLON:
      if (c == ':') tok.stepAndCapture(Token::COLON_COLON);
      else if (c == '=') tok.stepAndCapture(Token::COLON_EQUAL);
      else tok.capture(Token::COLON);
      break;

    case ST::DIGITS:
      if (isDigit(c)) tok.step(ST::DIGITS);
      else if (c == '.') tok.step(ST::DIGITS_DOT_DIGITS); 
      else tok.capture(Token::LIT_INT);
      break;
    
    case ST::DIGITS_DOT_DIGITS:
      if (isDigit(c)) tok.step(ST::DIGITS_DOT_DIGITS);
      else tok.capture(Token::LIT_DEC);
      break;

    case ST::EQUAL:
      if (c == '>') tok.stepAndCapture(Token::FATARROW);
      else if (c == '=') tok.stepAndCapture(Token::OP_EQ);
      else tok.capture(Token::EQUAL);
      break;

    case ST::FSLASH:
      if (c == '/') tok.step(ST::FSLASH_FSLASH);
      else if (c == '*') tok.step(ST::FSLASH_STAR);
      else if (c == '=') tok.stepAndCapture(Token::OP_NE);
      else tok.capture(Token::OP_DIV);
      break;

    case ST::FSLASH_FSLASH:
      if (c == '<') tok.step(ST::LINE_COMMENT_L);
      else if (c == '>') tok.step(ST::LINE_COMMENT_R);
      else if (c == '\n') tok.stepAndDiscard();   // discard non-doc comments
      else tok.step(ST::LINE_COMMENT);
      break;

    case ST::FSLASH_STAR:
      if (c == '*') tok.step(ST::FSLASH_STAR_STAR);
      else tok.step(ST::MULTILINE_COMMENT);
      break;

    case ST::FSLASH_STAR_STAR:
      if (c == '/') tok.stepAndDiscard();   // discard non-doc comments
      else tok.step(ST::MULTILINE_DOC_COMMENT);
      break;

    case ST::IDENT:
      if (isAlphaNumU(c)) tok.step(ST::IDENT);
      else tok.capture(identOrKeywordTy(tok.selection()));
      break;

    case ST::LINE_COMMENT:
      if (c == '\n') tok.stepAndDiscard();   // discard non-doc comments
      else tok.step(ST::LINE_COMMENT);
      break;

    case ST::LINE_COMMENT_L:
      if (c == '\n') {tok.capture(Token::DOC_COMMENT_L); tok.stepAndDiscard();}
      else tok.step(ST::LINE_COMMENT_L);
      break;

    case ST::LINE_COMMENT_R:
      if (c == '\n') {tok.capture(Token::DOC_COMMENT_R); tok.stepAndDiscard();}
      else tok.step(ST::LINE_COMMENT_R);
      break;

    case ST::MULTILINE_COMMENT:
      if (c == '*') tok.step(ST::MULTILINE_COMMENT_STAR);
      else tok.step(ST::MULTILINE_COMMENT);
      break;

    case ST::MULTILINE_COMMENT_STAR:
      if (c == '/') tok.stepAndDiscard();   // discard non-doc comments
      else if (c == '*') tok.step(ST::MULTILINE_COMMENT_STAR);
      else tok.step(ST::MULTILINE_COMMENT);
      break;

    case ST::MULTILINE_DOC_COMMENT:
      if (c == '*') tok.step(ST::MULTILINE_DOC_COMMENT_STAR);
      else tok.step(ST::MULTILINE_DOC_COMMENT);
      break;

    case ST::MULTILINE_DOC_COMMENT_STAR:
      if (c == '/') tok.stepAndCapture(Token::DOC_COMMENT_R);
      else if (c == '*') tok.step(ST::MULTILINE_DOC_COMMENT_STAR);
      else tok.step(ST::MULTILINE_DOC_COMMENT);
      break;

    case ST::STRING:
      if (c == '"') tok.stepAndCapture(Token::LIT_STRING);
      else if (c == '\\') tok.step(ST::STRING_BSLASH);
      else if (c == '\n') {
        err.append("Missing end quote of string literal.\n");
        err.append(tok.selectionLocation());
        return false;
      }
      else tok.step(ST::STRING);
      break;

    case ST::STRING_BSLASH:
      tok.step(ST::STRING);
      break;

    }
    return true;
  }

  /// @brief Called when end of file is reached. Captures one last token if
  /// necessary or produces an error for and unclosed comment or string.
  bool finalIteration() {
    switch (tok.state()) {
    case ST::ANGLE_L: tok.capture(Token::OP_LT); break;
    case ST::ANGLE_R: tok.capture(Token::OP_GT); break;
    case ST::BEGIN: break;
    case ST::COLON: tok.capture(Token::COLON); break;
    case ST::DIGITS: tok.capture(Token::LIT_INT); break;
    case ST::DIGITS_DOT_DIGITS: tok.capture(Token::LIT_DEC); break;
    case ST::EQUAL: tok.capture(Token::EQUAL); break;
    case ST::FSLASH: tok.capture(Token::OP_DIV); break;
    case ST::IDENT: tok.capture(identOrKeywordTy(tok.selection())); break;
    case ST::LINE_COMMENT_L: tok.capture(Token::DOC_COMMENT_L); break;
    case ST::LINE_COMMENT_R: tok.capture(Token::DOC_COMMENT_R); break;

    case ST::FSLASH_FSLASH:
    case ST::LINE_COMMENT: /* discard non-doc comments */ break;

    case ST::FSLASH_STAR:
    case ST::FSLASH_STAR_STAR:
    case ST::MULTILINE_COMMENT:
    case ST::MULTILINE_COMMENT_STAR:
    case ST::MULTILINE_DOC_COMMENT:
    case ST::MULTILINE_DOC_COMMENT_STAR:
      err.append("Unclosed comment at end of file.\n");
      err.append(tok.selectionLocation());
      return false;
    
    case ST::STRING:
    case ST::STRING_BSLASH:
      err.append("Unclosed string literal at end of file.\n");
      err.append(tok.selectionLocation());
      return false;
    
    }
    return true;
  }

  /// @brief If @p s is a keyword, returns the corresponding token tag,
  /// otherwise returns Token::IDENT.
  static Token::Tag identOrKeywordTy(llvm::StringRef s) {
    switch (s.size()) {
    case 1:
      if (s == "_") return Token::UNDERSCORE;
    case 2:
      if (s == "i8") return Token::KW_i8;
      if (s == "if") return Token::KW_IF;
      if (s == "of") return Token::KW_OF;
      return Token::IDENT;
    case 3:
      if (s == "f32") return Token::KW_f32;
      if (s == "f64") return Token::KW_f64;
      if (s == "i16") return Token::KW_i16;
      if (s == "i32") return Token::KW_i32;
      if (s == "i64") return Token::KW_i64;
      if (s == "let") return Token::KW_LET;
      if (s == "str") return Token::KW_STR;
      return Token::IDENT;
    case 4:
      if (s == "bool") return Token::KW_BOOL;
      if (s == "case") return Token::KW_CASE;
      if (s == "data") return Token::KW_DATA;
      if (s == "else") return Token::KW_ELSE;
      if (s == "func") return Token::KW_FUNC;
      if (s == "move") return Token::KW_MOVE;
      if (s == "proc") return Token::KW_PROC;
      if (s == "then") return Token::KW_THEN;
      if (s == "true") return Token::KW_TRUE;
      if (s == "unit") return Token::KW_UNIT;
      return Token::IDENT;
    case 5:
      if (s == "false") return Token::KW_FALSE;
      if (s == "match") return Token::KW_MATCH;
      return Token::IDENT;
    case 6:
      if (s == "borrow") return Token::KW_BORROW;
      if (s == "extern") return Token::KW_EXTERN;
      if (s == "module") return Token::KW_MODULE;
      if (s == "return") return Token::KW_RETURN;
      return Token::IDENT;
    default:
      return Token::IDENT;
    }
  }

  static bool isDigit(char c) {
    return '0' <= c && c <= '9';
  }

  static bool isAlpha(char c) {
    return ('A' <= c && c <= 'Z')
        || ('a' <= c && c <= 'z');
  }

  static bool isAlphaNumU(char c) {
    return ('A' <= c && c <= 'Z')
        || ('a' <= c && c <= 'z')
        || ('0' <= c && c <= '9')
        || c == '_';
  }

  static bool isWhitespace(char c) {
    return c == ' ' || c == '\n' || c == '\t';
  }
};

#endif