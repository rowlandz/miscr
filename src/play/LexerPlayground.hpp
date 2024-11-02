#ifndef LEXERPLAYGROUND
#define LEXERPLAYGROUND

#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/MemoryBuffer.h>
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
  llvm::StringRef line;

  while (true) {
    llvm::outs() << "\x1B[34m>\x1B[0m ";
    llvm::outs().flush();
    auto res = llvm::MemoryBuffer::getSTDIN();
    assert(res && "Could not read from stdin");
    line = res.get()->getBuffer();

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