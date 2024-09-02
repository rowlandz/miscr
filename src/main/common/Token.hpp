#ifndef COMMON_TOKEN
#define COMMON_TOKEN

enum TokenTy: unsigned char {
  TOK_IDENT,
  KW_BOOL, KW_CASE, KW_ELSE, KW_FALSE, KW_FUNC, KW_IF, KW_LET,
  KW_MATCH, KW_PROC, KW_RETURN, KW_STRING, KW_THEN, KW_TRUE, KW_UNIT,
  KW_i8, KW_i32, KW_f32, KW_f64,
  OP_ADD, OP_SUB, OP_MUL, OP_DIV,
  OP_EQ, OP_NEQ,
  LIT_DEC, LIT_INT, LIT_STRING,
  LPAREN, RPAREN, LBRACE, RBRACE,
  COLON, COMMA, EQUAL, FATARROW, SEMICOLON,
  COMMENT, DOC_COMMENT_L, DOC_COMMENT_R,
  END
};

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