#ifndef PARSERPLAYGROUND
#define PARSERPLAYGROUND

#include <functional>
#include <iostream>
#include "lexer/Lexer.hpp"
#include "parser/Parser.hpp"
#include "printers.hpp"

AST* parse_function(Parser& p, const char* whatToParse) {
  if (!strcmp(whatToParse, "decl")) return p.decl();
  if (!strcmp(whatToParse, "exp")) return p.exp();
  std::cout << "I don't know how to parse a " << whatToParse << std::endl;
  exit(1);
}

int play_with_parser(char* grammarElement, bool multilineInput) {
  std::string usrInput;
  std::string line;

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
    Parser parser(lexer.getTokens());
    AST* parsed = parse_function(parser, grammarElement);
    if (parsed != nullptr) {
      std::vector<bool> indents;
      print_parse_tree(parsed, indents);
      // TODO TODO TODO: FREE PARSED AST MEMORY!!!!!!!!!
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