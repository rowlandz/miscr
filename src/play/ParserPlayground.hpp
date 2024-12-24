#ifndef PARSERPLAYGROUND
#define PARSERPLAYGROUND

#include <functional>
#include <llvm/LineEditor/LineEditor.h>
#include "lexer/Lexer.hpp"
#include "parser/Parser.hpp"

AST* parse_function(Parser& p, llvm::StringRef whatToParse) {
  if (whatToParse == "decl") return p.decl();
  if (whatToParse == "exp") return p.exp();
  llvm::outs() << "I don't know how to parse a " << whatToParse << "\n";
  exit(1);
}

int play_with_parser(llvm::StringRef grammarElement) {
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
  AST* parsed = parse_function(parser, grammarElement);
  if (parsed != nullptr) {
    parsed->dump();
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