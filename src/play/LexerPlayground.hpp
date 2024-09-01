#ifndef LEXERPLAYGROUND
#define LEXERPLAYGROUND

#include <iostream>
#include "lexer/Lexer.hpp"

const char* TokenTyToString(TokenTy ty) {
  switch (ty) {
    case TOK_IDENT: return "TOK_IDENT";
    case KW_BOOL: return "KW_BOOL";
    case KW_CASE: return "KW_CASE";
    case KW_ELSE: return "KW_ELSE";
    case KW_FALSE: return "KW_FALSE";
    case KW_FUNC: return "KW_FUNC";
    case KW_IF: return "KW_IF";
    case KW_LET: return "KW_LET";
    case KW_MATCH: return "KW_MATCH";
    case KW_PROC: return "KW_PROC";
    case KW_RETURN: return "KW_RETURN";
    case KW_THEN: return "KW_THEN";
    case KW_TRUE: return "KW_TRUE";
    case KW_f32: return "KW_f32";
    case KW_f64: return "KW_f64";
    case KW_i8: return "KW_i8";
    case KW_i32: return "KW_i32";
    case OP_EQ: return "OP_EQ";
    case OP_NEQ: return "OP_NEQ";
    case OP_ADD: return "OP_ADD";
    case OP_SUB: return "OP_SUB";
    case OP_MUL: return "OP_MUL";
    case OP_DIV: return "OP_DIV";
    case LIT_DEC: return "LIT_DEC";
    case LIT_INT: return "LIT_INT";
    case LIT_STRING: return "LIT_STRING";
    case LPAREN: return "LPAREN";
    case RPAREN: return "RPAREN";
    case LBRACE: return "LBRACE";
    case RBRACE: return "RBRACE";
    case COLON: return "COLON";
    case COMMA: return "COMMA";
    case EQUAL: return "EQUAL";
    case FATARROW: return "FATARROW";
    case SEMICOLON: return "SEMICOLON";
    case COMMENT: return "COMMENT";
    case DOC_COMMENT_L: return "DOC_COMMENT_L";
    case DOC_COMMENT_R: return "DOC_COMMENT_R";
    case END: return "END";
  }
}

void print_tokens(std::vector<Token> &tokens) {
  for (auto token: tokens) {
    std::cout << TokenTyToString(token.ty) << "  ";
  }
  std::cout << std::endl;
}

void print_tokens_verbose(std::vector<Token> &tokens) {
  for (auto token: tokens) {
    char buffer[token.sz+1];
    strncpy(buffer, token.ptr, token.sz);
    buffer[token.sz] = '\0';
    printf("@ ln%3d, col%3d   %-15s   %s\n", token.row, token.col, TokenTyToString(token.ty), buffer);
  }
}

void play_with_lexer(bool verbose) {
  Lexer lexer;
  std::string line;

  while (!std::cin.eof()) {
    std::cout << "\x1B[34m>\x1B[0m " << std::flush;
    std::getline(std::cin, line);
    auto tokens = lexer.run(line.c_str());
    if (verbose)
      print_tokens_verbose(tokens);
    else
      print_tokens(tokens);
  }
}

#endif