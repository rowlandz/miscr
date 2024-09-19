#ifndef COMMON_TOKEN
#define COMMON_TOKEN

enum TokenTy: unsigned char {
  TOK_IDENT,
  KW_BOOL, KW_CASE, KW_ELSE, KW_EXTERN, KW_FALSE, KW_FUNC, KW_IF, KW_LET,
  KW_MATCH, KW_MODULE, KW_NAMESPACE, KW_OF, KW_PROC, KW_RETURN, KW_STRING, KW_THEN,
  KW_TRUE, KW_UNIT,
  KW_i8, KW_i32, KW_f32, KW_f64,
  OP_ADD, OP_SUB, OP_MUL, OP_DIV,
  OP_EQ, OP_NEQ,
  LIT_DEC, LIT_INT, LIT_STRING,
  LPAREN, RPAREN, LBRACE, RBRACE, LBRACKET, RBRACKET,
  AMP, COLON_COLON, COLON, COLON_EQUAL, COMMA, DOT, EQUAL, EXCLAIM, FATARROW,
  HASH, SEMICOLON, UNDERSCORE,
  COMMENT, DOC_COMMENT_L, DOC_COMMENT_R,
  END
};

const char* TokenTyToString(TokenTy ty) {
  switch (ty) {
    case TOK_IDENT:       return "TOK_IDENT";
    case KW_UNIT:         return "KW_UNIT";
    case KW_BOOL:         return "KW_BOOL";
    case KW_CASE:         return "KW_CASE";
    case KW_ELSE:         return "KW_ELSE";
    case KW_EXTERN:       return "KW_EXTERN";
    case KW_FALSE:        return "KW_FALSE";
    case KW_FUNC:         return "KW_FUNC";
    case KW_IF:           return "KW_IF";
    case KW_LET:          return "KW_LET";
    case KW_MATCH:        return "KW_MATCH";
    case KW_MODULE:       return "KW_MODULE";
    case KW_NAMESPACE:    return "KW_NAMESPACE";
    case KW_OF:           return "KW_OF";
    case KW_PROC:         return "KW_PROC";
    case KW_RETURN:       return "KW_RETURN";
    case KW_THEN:         return "KW_THEN";
    case KW_TRUE:         return "KW_TRUE";
    case KW_f32:          return "KW_f32";
    case KW_f64:          return "KW_f64";
    case KW_i8:           return "KW_i8";
    case KW_i32:          return "KW_i32";
    case KW_STRING:       return "KW_STRING";
    case OP_EQ:           return "OP_EQ";
    case OP_NEQ:          return "OP_NEQ";
    case OP_ADD:          return "OP_ADD";
    case OP_SUB:          return "OP_SUB";
    case OP_MUL:          return "OP_MUL";
    case OP_DIV:          return "OP_DIV";
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
    case COLON_COLON:     return "COLON_COLON";
    case COLON:           return "COLON";
    case COLON_EQUAL:     return "COLON_EQUAL";
    case COMMA:           return "COMMA";
    case DOT:             return "DOT";
    case EQUAL:           return "EQUAL";
    case EXCLAIM:         return "EXCLAIM";
    case FATARROW:        return "FATARROW";
    case HASH:            return "HASH";
    case SEMICOLON:       return "SEMICOLON";
    case UNDERSCORE:      return "UNDERSCORE";
    case COMMENT:         return "COMMENT";
    case DOC_COMMENT_L:   return "DOC_COMMENT_L";
    case DOC_COMMENT_R:   return "DOC_COMMENT_R";
    case END:             return "END";
  }
}

/**
 * A single "unit of meaning." The lexer turns a program into a sequence of
 * these and the parser turns that sequence into an AST.
 */
typedef struct {
  const char* ptr;
  unsigned short row;
  unsigned short col;
  unsigned short sz;
  TokenTy ty;
} Token;

#endif