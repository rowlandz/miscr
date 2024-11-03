#ifndef LEXERPLAYGROUND
#define LEXERPLAYGROUND

#include <llvm/LineEditor/LineEditor.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/FormatVariadic.h>
#include "lexer/Lexer.hpp"

void print_tokens(const std::vector<Token>& tokens) {
  for (Token token: tokens) {
    llvm::outs() << token.tagAsString() << "  ";
  }
  llvm::outs() << "\n";
}

void print_tokens_verbose(const std::vector<Token>& tokens) {
  for (Token token: tokens) {
    llvm::outs() << llvm::formatv("r{0,-3} c{1,-3}   {2,-15}   {3}\n",
      token.loc.row,
      token.loc.col,
      token.tagAsString(),
      token.asString().data());
  }
}

void play_with_lexer(bool verbose) {
  llvm::LineEditor lineEditor("");

  while (true) {
    llvm::outs() << "\x1B[34m";
    auto maybeLine = lineEditor.readLine();
    llvm::outs() << "\x1B[0m";
    assert(maybeLine.hasValue() && "Could not read line from stdin");
    llvm::StringRef line = maybeLine.getValue();

    Lexer lexer(line.data());
    if (!lexer.run()) {
      llvm::outs() <<
        lexer.getError().render(line.data(), lexer.getLocationTable()) << "\n";
      continue;
    }
    if (verbose)
      print_tokens_verbose(lexer.getTokens());
    else
      print_tokens(lexer.getTokens());
  }
}

#endif