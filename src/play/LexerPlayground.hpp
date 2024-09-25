#ifndef LEXERPLAYGROUND
#define LEXERPLAYGROUND

#include "lexer/Lexer.hpp"

void print_tokens(const std::vector<Token>* tokens) {
  for (auto token: *tokens) {
    std::cout << TokenTyToString(token.ty) << "  ";
  }
  std::cout << std::endl;
}

void print_tokens_verbose(const std::vector<Token>* tokens) {
  for (auto token: *tokens) {
    char buffer[token.sz+1];
    strncpy(buffer, token.ptr, token.sz);
    buffer[token.sz] = '\0';
    printf("@ ln%3d, col%3d   %-15s   %s\n", token.row, token.col, TokenTyToString(token.ty), buffer);
  }
}

void play_with_lexer(bool verbose) {
  std::string line;

  while (!std::cin.eof()) {
    std::cout << "\x1B[34m>\x1B[0m " << std::flush;
    std::getline(std::cin, line);

    Lexer lexer(line.c_str());
    if (!lexer.run()) {
      std::cout << lexer.getError().render(line.c_str(), lexer.getLocationTable()) << std::endl;
      continue;
    }
    if (verbose)
      print_tokens_verbose(lexer.getTokens());
    else
      print_tokens(lexer.getTokens());
  }
}

#endif