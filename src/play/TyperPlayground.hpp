#ifndef TYPERPLAYGROUND
#define TYPERPLAYGROUND

#include <iostream>
#include "lexer/Lexer.hpp"
#include "parser/Parser.hpp"
#include "typer/Typer.hpp"
#include "printers.hpp"

int play_with_typer(char* grammarElement) {
  Lexer lexer;
  std::string line;

  unsigned int(Parser::*chosenParseFunction)();
  void(Typer::*chosenTypeFunction)(unsigned int);
  if (!strcmp(grammarElement, "decl")) {
    chosenParseFunction = &Parser::funcOrProc;
    chosenTypeFunction = &Typer::tyFuncOrProc;
  } else if (!strcmp(grammarElement, "exp")) {
    chosenParseFunction = &Parser::exp;
    chosenTypeFunction = &Typer::voidTyExp;
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
    auto tokens = lexer.run(line.c_str());
    Parser parser(tokens);

    unsigned int parsed = (parser.*chosenParseFunction)();
    if (IS_ERROR(parsed)) {
      std::cout << "Parse error" << std::endl;
      continue;
    }

    Typer typer(&parser.m);
    (typer.*chosenTypeFunction)(parsed);

    std::vector<bool> indents;
    print_parse_tree(parser.m, parsed, indents);
  }

  return 0;
}

#endif