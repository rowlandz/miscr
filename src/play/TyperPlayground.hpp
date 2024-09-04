#ifndef TYPERPLAYGROUND
#define TYPERPLAYGROUND

#include <iostream>
#include "lexer/Lexer.hpp"
#include "parser/Parser.hpp"
#include "typer/Typer.hpp"
#include "printers.hpp"

int play_with_typer(char* grammarElement) {
  std::string line;

  unsigned int(Parser::*chosenParseFunction)();
  void (Typer::*chosenTypeFunction)(unsigned int);
  if (!strcmp(grammarElement, "decl")) {
    chosenParseFunction = &Parser::decl;
    chosenTypeFunction = &Typer::typeDecl;
  } else if (!strcmp(grammarElement, "exp")) {
    chosenParseFunction = &Parser::exp;
    chosenTypeFunction = &Typer::typeExp;
  } else if (!strcmp(grammarElement, "stmt")) {
    chosenParseFunction = &Parser::stmt;
    printf("Can't type a stmt right now!\n");
    exit(1);
  } else {
    std::cout << "I don't know how to typecheck a " << grammarElement << std::endl;
    return 1;
  }

  while (!std::cin.eof()) {
    std::cout << "\x1B[34m>\x1B[0m " << std::flush;
    std::getline(std::cin, line);

    Lexer lexer(line.c_str());
    if (!lexer.run()) {
      std::cout << lexer.getError().render(line.c_str(), lexer.getLocationTable()) << std::endl;
      continue;
    }

    NodeManager m;
    Parser parser(&m, lexer.getTokens());
    unsigned int parsed = (parser.*chosenParseFunction)();
    if (IS_ERROR(parsed)) {
      std::cout << parser.getError().render(line.c_str(), lexer.getLocationTable()) << std::endl;
      continue;
    }

    Typer typer(&m);
    (typer.*chosenTypeFunction)(parsed);
    if (typer.unifier.errors.size() == 0) {
      std::vector<bool> indents;
      print_parse_tree(m, parsed, indents);
    } else {
      for (auto err : typer.unifier.errors) {
        std::cout << err.render(line.c_str(), lexer.getLocationTable()) << std::endl;
      }
    }
  }

  return 0;
}

#endif