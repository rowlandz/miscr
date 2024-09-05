//#include <llvm/IR/IRBuilder.h>
#include <cstdio>
#include "lexer/Lexer.hpp"
#include "parser/Parser.hpp"
#include "typer/Typer.hpp"
#include "codegen/Codegen.hpp"

int main() {

  const char* text =
    "module Main {\n"
    "  extern proc puts(s: string): i32;\n"
    "  func main(): i32 = puts(\"hello\");\n"
    "}\n"
    ;


  Lexer lexer(text);
  lexer.run();

  NodeManager m;
  Parser parser(&m, lexer.getTokens());
  unsigned int parsed = parser.decl();
  if (IS_ERROR(parsed)) {
    printf("%s", parser.getError().render(text, lexer.getLocationTable()).c_str());
    exit(1);
  }

  Typer typer(&m);
  typer.typeDecl(parsed);
  if (typer.unifier.errors.size() > 0) {
    for (auto err : typer.unifier.errors) {
      printf("%s", err.render(text, lexer.getLocationTable()).c_str());
    }
    exit(1);
  }

  Codegen codegen(&m, &typer.ont);
  codegen.genDecl(parsed);

  std::string generatedCode;
  llvm::raw_string_ostream os(generatedCode);
  os << *codegen.mod;
  os.flush();
  printf("%s\n", generatedCode.c_str());

  //printf("This doesn't do anything yet. Use the playground and unit tests instead.\n");
  return 0;
}