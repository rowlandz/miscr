#include "LexerPlayground.hpp"
#include "ParserPlayground.hpp"
#include "TyperPlayground.hpp"

char help_message[] =
  "Welcome to the playground!\n"
  "\n"
  "USAGE\n"
  "    ./playground lexer [-v]\n"
  "    ./playground parser (decl|exp)\n"
  "    ./playground typer (decl|exp)\n"
  "\n"
  "OPTIONS\n"
  "    -v   verbose output\n"
  ;


int main(int argc, char* argv[]) {
  
  if (argc <= 1) {
    llvm::outs() << help_message;
  }

  else if (!strcmp(argv[1], "lexer")) {
    play_with_lexer(argc >= 3 && !strcmp(argv[2], "-v"));
  }

  else if (!strcmp(argv[1], "parser")) {
    return play_with_parser(argv[2]);
  }

  else if (!strcmp(argv[1], "typer")) {
    if (argc >= 3) {
      return play_with_typer(argv[2]);
    }
  }

  else {
    llvm::outs() << "Unrecognized arguments\n";
    return 1;
  }

  return 0;
}