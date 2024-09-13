#ifndef TYPERPLAYGROUND
#define TYPERPLAYGROUND

#include <iostream>
#include "lexer/Lexer.hpp"
#include "parser/Parser.hpp"
#include "typer/Typer.hpp"
#include "printers.hpp"

Addr<AST> parse_function1(Parser& p, const char* whatToParse) {
  if (!strcmp(whatToParse, "decl")) return p.decl().upcast<AST>();
  if (!strcmp(whatToParse, "exp")) return p.exp().upcast<AST>();
  std::cout << "I don't know how to parse a " << whatToParse << std::endl;
  exit(1);
}

void type_function(Typer& t, Addr<AST> _ast, const char* whatToType) {
  if (!strcmp(whatToType, "decl")) { t.typeDecl(_ast.UNSAFE_CAST<Decl>()); return; }
  if (!strcmp(whatToType, "exp")) { t.typeExp(_ast.UNSAFE_CAST<Exp>()); return; }
  std::cout << "I don't know how to type a " << whatToType << std::endl;
  exit(1);
}

int play_with_typer(char* grammarElement) {
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
    ASTContext m;
    Parser parser(&m, lexer.getTokens());
    Addr<AST> parsed = parse_function1(parser, grammarElement);
    if (!parsed.isError()) {

      Typer typer(&m);
      type_function(typer, parsed, grammarElement);
      for (auto err : typer.unifier.errors) {
        std::cout << err.render(usrInput.c_str(), lexer.getLocationTable());
      }
      if (typer.unifier.errors.size() == 0) {
        std::cout << "No errors!" << std::endl;
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