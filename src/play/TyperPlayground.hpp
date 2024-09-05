#ifndef TYPERPLAYGROUND
#define TYPERPLAYGROUND

#include <iostream>
#include "lexer/Lexer.hpp"
#include "parser/Parser.hpp"
#include "typer/Typer.hpp"
#include "printers.hpp"

int play_with_typer(char* grammarElement) {
  std::string usrInput;
  std::string line;

  unsigned int(Parser::*chosenParseFunction)();
  void (Typer::*chosenTypeFunction)(unsigned int);
  if (!strcmp(grammarElement, "decl")) {
    chosenParseFunction = &Parser::decl;
    chosenTypeFunction = &Typer::typeDecl;
  } else if (!strcmp(grammarElement, "exp")) {
    chosenParseFunction = &Parser::exp;
    chosenTypeFunction = &Typer::typeExp;
  } else {
    std::cout << "I don't know how to typecheck a " << grammarElement << std::endl;
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
      Typer typer(&m);
      (typer.*chosenTypeFunction)(parsed);
      if (typer.unifier.errors.size() == 0) {
        std::vector<bool> indents;
        print_parse_tree(m, parsed, indents);
      } else {
        for (auto err : typer.unifier.errors) {
          std::cout << err.render(usrInput.c_str(), lexer.getLocationTable()) << std::endl;
        }
      }
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