#ifndef TYPERPLAYGROUND
#define TYPERPLAYGROUND

#include <iostream>
#include "lexer/Lexer.hpp"
#include "parser/Parser.hpp"
#include "typer/Typer.hpp"

AST* parse_function1(Parser& p, llvm::StringRef whatToParse) {
  if (whatToParse == "decl") return p.decl();
  if (whatToParse == "exp") return p.exp();
  llvm::outs() << "I don't know how to parse a " << whatToParse << "\n";
  exit(1);
}

void type_function(Typer& t, AST* _ast, const char* whatToType) {
  if (!strcmp(whatToType, "decl")) {
    t.typeDecl(static_cast<Decl*>(_ast));
  }
  else if (!strcmp(whatToType, "exp")) {
    t.typeExp(static_cast<Exp*>(_ast));
  } else {
    llvm::outs() << "I don't know how to type a " << whatToType << "\n";
    exit(1);
  }
}

int play_with_typer(char* grammarElement) {
  llvm::LineEditor lineEditor("");

  std::string usrInput;
  std::optional<std::string> maybeLine;
  llvm::StringRef line;

next_input:
  usrInput.clear();

next_line:
  llvm::outs() << "\x1B[34m";
  maybeLine = lineEditor.readLine();
  llvm::outs() << "\x1B[0m";
  assert(maybeLine.has_value() && "Could not read line from stdin");
  line = maybeLine.value();
  usrInput += line; usrInput += "\n";

check_input:
  LocationTable LT(usrInput.c_str());
  auto tokens = Lexer(usrInput.c_str(), &LT).run();
  Parser parser(tokens);
  AST* parsed = parse_function1(parser, grammarElement);
  if (parsed != nullptr) {

    Typer typer;
    type_function(typer, parsed, grammarElement);
    for (auto err : typer.getErrors()) {
      std::cout << err.render(usrInput.c_str(), LT);
    }
    if (typer.getErrors().size() == 0) {
      parsed->dump();
    }

    parsed->deleteRecursive();
    goto next_input;
  } else if (line.size() == 0) {
    llvm::outs()
      << parser.getError().render(usrInput.c_str(), LT)
      << "\n";
    goto next_input;
  } else goto next_line;
}

#endif