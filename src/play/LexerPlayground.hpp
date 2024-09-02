#ifndef LEXERPLAYGROUND
#define LEXERPLAYGROUND

#include <iostream>
#include "lexer/Lexer.hpp"

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