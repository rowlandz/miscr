#ifndef PARSERPLAYGROUND
#define PARSERPLAYGROUND

#include <functional>
#include <iostream>
#include "common/Node.hpp"
#include "lexer/Lexer.hpp"
#include "parser/Parser.hpp"
#include "printers.hpp"

int play_with_parser(char* grammarElement) {
  Lexer lexer;
  std::string line;

  unsigned int(Parser::*chosenParseFunction)();
  if (!strcmp(grammarElement, "decl")) chosenParseFunction = &Parser::funcOrProc;
  else if (!strcmp(grammarElement, "exp")) chosenParseFunction = &Parser::exp;
  else if (!strcmp(grammarElement, "stmt")) chosenParseFunction = &Parser::stmt;
  else {
    std::cout << "I don't know how to parse a " << grammarElement << std::endl;
    return 1;
  }

  while (!std::cin.eof()) {
    std::cout << "\x1B[34m>\x1B[0m " << std::flush;
    std::getline(std::cin, line);
    auto tokens = lexer.run(line.c_str());
    Parser parser(tokens);

    unsigned int parsed = (parser.*chosenParseFunction)();
    if (IS_ERROR(parsed))
      std::cout << "Parse error" << std::endl;
    else {
      std::vector<bool> indents;
      print_parse_tree(parser.m, parsed, indents);
    }
  }

  return 0;
}

#endif