//#include <llvm/IR/IRBuilder.h>
#include <cstdio>
#include "lexer/Lexer.hpp"
#include "parser/Parser.hpp"
#include "codegen/Codegen.hpp"

int main() {

  Lexer lexer("proc main(): string = \"Hello there!\";");
  lexer.run();

  NodeManager m;
  Parser parser(&m, lexer.getTokens());
  unsigned int parsed = parser.decl();

  Codegen codegen(&m);
  codegen.genProc(parsed);

  std::string generatedCode;
  llvm::raw_string_ostream os(generatedCode);
  os << *codegen.mod;
  os.flush();
  printf("%s\n", generatedCode.c_str());

  //printf("This doesn't do anything yet. Use the playground and unit tests instead.\n");
  return 0;
}