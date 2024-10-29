#include <llvm/Support/MemoryBuffer.h>
#include "lexer/Lexer.hpp"
#include "parser/Parser.hpp"
#include "typer/Typer.hpp"
#include "codegen/Codegen.hpp"
#include "borrowchecker/AccessPath.hpp"
#include "borrowchecker/BorrowChecker.hpp"

int main(int argc, char* argv[]) {

  if (argc < 2) {
    llvm::outs() << "Need at least one cmd arg\n";
    return 1;
  }

  auto Content = llvm::MemoryBuffer::getFile(argv[1], true);
  assert(Content && "Could not read file");
  llvm::StringRef text = Content.get()->getBuffer();
  
  Lexer lexer(text);
  if (!lexer.run()) {
    llvm::outs() << lexer.getError().render(text.data(), lexer.getLocationTable());
    return 1;
  }

  Parser parser(lexer.getTokens());
  DeclList* decls = parser.decls0();
  if (decls == nullptr) {
    llvm::outs() << parser.getError().render(text.data(), lexer.getLocationTable());
    exit(1);
  }

  Typer typer;
  typer.typeDeclList(decls);
  if (!typer.errors.empty()) {
    for (auto err : typer.errors) {
      llvm::outs() << err.render(text.data(), lexer.getLocationTable());
    }
    exit(1);
  }

  BorrowChecker bc(typer.getTypeContext());
  bc.checkDecls(decls);
  if (!bc.errors.empty()) {
    for (auto err : bc.errors) {
      llvm::outs() << err.render(text.data(), lexer.getLocationTable());
    }
    exit(1);
  }

  Codegen codegen(typer.getTypeContext(), typer.ont);
  codegen.genDeclList(decls);
  codegen.mod->setModuleIdentifier(argv[1]);
  codegen.mod->setSourceFileName(argv[1]);

  llvm::outs() << *codegen.mod;

  std::error_code EC;
  llvm::raw_fd_ostream outDotLL("output.ll", EC);

  outDotLL << *codegen.mod;
  outDotLL.flush();

  delete decls;

  return 0;
}