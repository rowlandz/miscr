#ifndef SEMAPLAYGROUND
#define SEMAPLAYGROUND

#include <iostream>
#include "lexer/Lexer.hpp"
#include "parser/Parser.hpp"
#include "sema/Sema.hpp"

AST* parse_function1(Parser& p, llvm::StringRef whatToParse) {
  if (whatToParse == "decl") return p.decl();
  if (whatToParse == "exp") return p.exp();
  llvm::outs() << "I don't know how to parse a " << whatToParse << "\n";
  exit(1);
}

void type_function(Sema& sema, AST* _ast, const char* whatToType) {
  if (!strcmp(whatToType, "decl")) {
    sema.run(static_cast<Decl*>(_ast), "global");
  }
  else if (!strcmp(whatToType, "exp")) {
    sema.analyzeExp(static_cast<Exp*>(_ast), "global");
  } else {
    llvm::outs() << "I don't know how to analyze a " << whatToType << "\n";
    exit(1);
  }
}

int play_with_sema(char* grammarElement) {
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

    Sema sema;
    type_function(sema, parsed, grammarElement);
    for (auto err : sema.getErrors()) {
      std::cout << err.render(usrInput.c_str(), LT);
    }
    if (sema.getErrors().size() == 0) {
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