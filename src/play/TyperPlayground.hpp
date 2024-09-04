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
  void(*chosenTypeFunction)(unsigned int, NodeManager*);
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
    if (!lexer.run(line.c_str())) {
      std::cout << lexer.getError().render() << std::endl;
      continue;
    }
    auto tokens = lexer.getTokens();
    Parser parser(tokens);

    unsigned int parsed = (parser.*chosenParseFunction)();
    if (IS_ERROR(parsed)) {
      std::cout << "Parse error" << std::endl;
      continue;
    }

    chosenTypeFunction(parsed, &parser.m);

    std::vector<bool> indents;
    print_parse_tree(parser.m, parsed, indents);
    
    // for (auto err : typer.errors) {
    //   printf("%s\n", err.c_str());
    // }
  }

  return 0;
}

#endif