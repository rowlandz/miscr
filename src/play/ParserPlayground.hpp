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

next_input:
  if (std::cin.eof()) return 0;
  usrInput.clear();
  std::cout << "\x1B[34m>\x1B[0m " << std::flush;
  std::getline(std::cin, line);
  usrInput.append(line + "\n");
  goto check_input;

next_line:
  std::cout << "\x1B[34m|\x1B[0m " << std::flush;
  std::getline(std::cin, line);
  usrInput.append(line + "\n");

check_input:
  Lexer lexer(usrInput.c_str());
  if (lexer.run()) {
    NodeManager m;
    Parser parser(&m, lexer.getTokens());
    unsigned int parsed = (parser.*chosenParseFunction)();
    if (!IS_ERROR(parsed)) {
      std::vector<bool> indents;
      print_parse_tree(m, parsed, indents);
      goto next_input;
    } else if (line.size() == 0) {
      std::cout << parser.getError().render(usrInput.c_str(), lexer.getLocationTable()) << std::endl;
      goto next_input;
    } else goto next_line;
  } else if (line.size() == 0) {
    std::cout << lexer.getError().render(usrInput.c_str(), lexer.getLocationTable()) << std::endl;
    goto next_input;
  } else goto next_line;
}

#endif