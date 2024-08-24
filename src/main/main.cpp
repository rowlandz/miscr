//#include <llvm/IR/IRBuilder.h>
#include <cstdio>
#include "lexer/Lexer.hpp"
#include "parser/Parser.hpp"
#include "codegen/Codegen.hpp"

int main() {

  printf("Size of Location: %lu\n", sizeof(Location));
  printf("Size of Node: %lu\n", sizeof(Node));

  auto tokens = Lexer().run("proc main(x: i32, y: i32): i32 = x + y + 1;");
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