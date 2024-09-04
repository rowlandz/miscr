#ifndef PARSERPLAYGROUND
#define PARSERPLAYGROUND

#include <functional>
#include <iostream>
#include "common/Node.hpp"
#include "common/LocatedError.hpp"
#include "lexer/Lexer.hpp"
#include "parser/Parser.hpp"
#include "printers.hpp"

int play_with_parser(char* grammarElement, bool multilineInput) {
  Lexer lexer;
  std::string usrInput;
  std::string line;

  unsigned int(Parser::*chosenParseFunction)();
  if (!strcmp(grammarElement, "decl")) chosenParseFunction = &Parser::decl;
  else if (!strcmp(grammarElement, "exp")) chosenParseFunction = &Parser::exp;
  else if (!strcmp(grammarElement, "stmt")) chosenParseFunction = &Parser::stmt;
  else {
    std::cout << "I don't know how to parse a " << grammarElement << std::endl;
    return 1;
  }

  while (!std::cin.eof()) {
    std::cout << "\x1B[34m>\x1B[0m " << std::flush;
    
    usrInput.clear();
    std::getline(std::cin, line);
    while (line.size() > 0) {
      usrInput.append(line);
      usrInput.append("\n");
      std::cout << "\x1B[34m|\x1B[0m " << std::flush;
      std::getline(std::cin, line);
    }

    if (!lexer.run(usrInput.c_str())) {
      std::cout << lexer.getError().render() << std::endl;
      continue;
    }

    std::vector<Token> tokens = lexer.getTokens();
    Parser parser(tokens);

    unsigned int parsed = (parser.*chosenParseFunction)();
    if (IS_ERROR(parsed)) {
      std::cout << parser.getError().render() << std::endl;
    } else {
      std::vector<bool> indents;
      print_parse_tree(parser.m, parsed, indents);
    }
  }

  return 0;
}

#endif