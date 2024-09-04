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

    Lexer lexer(usrInput.c_str());
    if (!lexer.run()) {
      std::cout << lexer.getError().render(usrInput.c_str(), lexer.getLocationTable()) << std::endl;
      continue;
    }

    NodeManager m;
    Parser parser(&m, lexer.getTokens());
    unsigned int parsed = (parser.*chosenParseFunction)();
    if (IS_ERROR(parsed)) {
      std::cout << parser.getError().render(usrInput.c_str(), lexer.getLocationTable()) << std::endl;
    } else {
      std::vector<bool> indents;
      print_parse_tree(m, parsed, indents);
    }
  }

  return 0;
}

#endif