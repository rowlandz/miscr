#ifndef COMMON_TOKEN
#define COMMON_TOKEN

#include "common/Location.hpp"

/// @brief A lexical "unit of meaning" such as an operator, keyword, or
/// parenthesis. The Lexer turns a string of source code into a vector of these.
class Token {
public:

  /// @brief The type of token.
  enum Tag : unsigned char {

    // error token
    ERROR,

    // keywords
    KW_BOOL, KW_BORROW, KW_CASE, KW_DATA, KW_ELSE, KW_EXTERN, KW_f32, KW_f64,
    KW_FALSE, KW_FUNC, KW_i8, KW_i16, KW_i32, KW_i64, KW_IF, KW_LET, KW_MATCH,
    KW_MODULE, KW_MOVE, KW_OF, KW_PROC, KW_RETURN, KW_STR, KW_THEN, KW_TRUE,
    KW_UNIT, KW_WHILE,

    // operators
    OP_ADD, OP_DIV, OP_EQ, OP_GE, OP_GT, OP_LE, OP_LT, OP_MOD, OP_MUL, OP_NE,
    OP_OR, OP_SUB,

    // literal values
    LIT_DEC, LIT_INT, LIT_STRING,

    // comments
    COMMENT, DOC_COMMENT_L, DOC_COMMENT_R,

    // grouping symbols
    LPAREN, RPAREN, LBRACE, RBRACE, LBRACKET, RBRACKET,

    // other
    AMP, ARROW, COLON_COLON, COLON, COLON_EQUAL, COMMA, DOT, ELLIPSIS, EQUAL,
    EXCLAIM, FATARROW, HASH, IDENT, SEMICOLON, TILDE, UNDERSCORE,

    // end-of-file indicator
    END
  };

  const Tag tag;
  const char* const ptr;
  const Location loc;

  Token(Tag tag, const char* ptr, Location loc)
    : tag(tag), ptr(ptr), loc(loc) {}

  /// @brief Returns a string matching the tag's name in the Tag enum.
  const char* tagAsString() const { return tagToString(tag); }

  /// @brief Returns the token as it appears in the source code.
  llvm::StringRef asStringRef() const { return llvm::StringRef(ptr, loc.sz); }

  static const char* tagToString(Tag tag) {
    switch (tag) {
    case ERROR:           return "ERROR";
    case IDENT:           return "IDENT";
    case KW_UNIT:         return "KW_UNIT";
    case KW_BOOL:         return "KW_BOOL";
    case KW_BORROW:       return "KW_BORROW";
    case KW_CASE:         return "KW_CASE";
    case KW_DATA:         return "KW_DATA";
    case KW_ELSE:         return "KW_ELSE";
    case KW_EXTERN:       return "KW_EXTERN";
    case KW_FALSE:        return "KW_FALSE";
    case KW_FUNC:         return "KW_FUNC";
    case KW_IF:           return "KW_IF";
    case KW_LET:          return "KW_LET";
    case KW_MATCH:        return "KW_MATCH";
    case KW_MODULE:       return "KW_MODULE";
    case KW_MOVE:         return "KW_MOVE";
    case KW_OF:           return "KW_OF";
    case KW_PROC:         return "KW_PROC";
    case KW_RETURN:       return "KW_RETURN";
    case KW_THEN:         return "KW_THEN";
    case KW_TRUE:         return "KW_TRUE";
    case KW_f32:          return "KW_f32";
    case KW_f64:          return "KW_f64";
    case KW_i8:           return "KW_i8";
    case KW_i16:          return "KW_i16";
    case KW_i32:          return "KW_i32";
    case KW_i64:          return "KW_i64";
    case KW_STR:          return "KW_STR";
    case KW_WHILE:        return "KW_WHILE";
    case OP_GE:           return "OP_GE";
    case OP_GT:           return "OP_GT";
    case OP_LE:           return "OP_LE";
    case OP_LT:           return "OP_LT";
    case OP_EQ:           return "OP_EQ";
    case OP_NE:           return "OP_NE";
    case OP_OR:           return "OP_OR";
    case OP_ADD:          return "OP_ADD";
    case OP_SUB:          return "OP_SUB";
    case OP_MUL:          return "OP_MUL";
    case OP_DIV:          return "OP_DIV";
    case OP_MOD:          return "OP_MOD";
    case LIT_DEC:         return "LIT_DEC";
    case LIT_INT:         return "LIT_INT";
    case LIT_STRING:      return "LIT_STRING";
    case LPAREN:          return "LPAREN";
    case RPAREN:          return "RPAREN";
    case LBRACE:          return "LBRACE";
    case RBRACE:          return "RBRACE";
    case LBRACKET:        return "LBRACKET";
    case RBRACKET:        return "RBRACKET";
    case AMP:             return "AMP";
    case ARROW:           return "ARROW";
    case COLON_COLON:     return "COLON_COLON";
    case COLON:           return "COLON";
    case COLON_EQUAL:     return "COLON_EQUAL";
    case COMMA:           return "COMMA";
    case DOT:             return "DOT";
    case ELLIPSIS:        return "ELLIPSIS";
    case EQUAL:           return "EQUAL";
    case EXCLAIM:         return "EXCLAIM";
    case FATARROW:        return "FATARROW";
    case HASH:            return "HASH";
    case SEMICOLON:       return "SEMICOLON";
    case TILDE:           return "TILDE";
    case UNDERSCORE:      return "UNDERSCORE";
    case COMMENT:         return "COMMENT";
    case DOC_COMMENT_L:   return "DOC_COMMENT_L";
    case DOC_COMMENT_R:   return "DOC_COMMENT_R";
    case END:             return "END";
    }
    llvm_unreachable("Token::tagToString() unhandled switch case");
    return nullptr;
  }
};

#endif