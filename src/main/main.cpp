//#include <llvm/IR/IRBuilder.h>
#include <cstdio>
#include "lexer/Lexer.hpp"
#include "parser/Parser.hpp"
#include "codegen/Codegen.hpp"

int main() {

  auto tokens = Lexer().run("proc main(x: i32): i32 = { let y = x + 1; x * y; };");
  Parser parser(tokens);
  auto parsed = parser.funcOrProc();
  Codegen codegen(&parser.m);
  codegen.genProc(parsed);

  std::string generatedCode;
  llvm::raw_string_ostream os(generatedCode);
  os << *codegen.mod;
  os.flush();
  printf("%s\n", generatedCode.c_str());

  //printf("This doesn't do anything yet. Use the playground and unit tests instead.\n");
  return 0;
}