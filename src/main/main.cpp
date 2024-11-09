//=== src/main/main.cpp ======================================================//
//
// The main entry point for the MiSCR compiler.
//============================================================================//
#include <unistd.h>
#include <wait.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/MemoryBuffer.h>
#include "lexer/Lexer.hpp"
#include "parser/Parser.hpp"
#include "typer/Typer.hpp"
#include "borrowchecker/BorrowChecker.hpp"
#include "codegen/Codegen.hpp"

llvm::cl::OptionCategory miscrOptions("MiSCR Options");

llvm::cl::opt<std::string> inFileOpt(llvm::cl::Positional,
  llvm::cl::desc("FILE.miscr"), llvm::cl::Required,
  llvm::cl::cat(miscrOptions));

llvm::cl::opt<bool> skipBorrowCheckingOpt("b",
  llvm::cl::desc("Skip borrow checking"),
  llvm::cl::cat(miscrOptions));

llvm::cl::opt<bool> emitLLVMOpt("emit-llvm",
  llvm::cl::desc("Emit output as LLVM IR"),
  llvm::cl::cat(miscrOptions));

llvm::cl::opt<std::string> outFileOpt("o",
  llvm::cl::desc("Write output to FILE"),
  llvm::cl::value_desc("FILE"),
  llvm::cl::cat(miscrOptions));

int main(int argc, char** argv) {

  // parse command-line options
  llvm::cl::HideUnrelatedOptions(miscrOptions);
  llvm::cl::SetVersionPrinter([](llvm::raw_ostream& out){
    out << "miscrc 0.0.1\n";
  });
  llvm::cl::ParseCommandLineOptions(argc, argv);

  // read MiSCR source code from input file
  auto maybeSrcCode = llvm::MemoryBuffer::getFile(inFileOpt.getValue(), true);
  if (!maybeSrcCode) {
    llvm::errs() << "Could not read file " << inFileOpt.getValue() << "\n";
    return 1;
  }
  llvm::StringRef srcCode = maybeSrcCode.get()->getBuffer();
  
  // lex
  Lexer lexer(srcCode);
  const LocationTable& locTab = lexer.getLocationTable();
  if (!lexer.run()) {
    llvm::errs() << lexer.getError().render(srcCode.data(), locTab);
    return 1;
  }

  // parse
  Parser parser(lexer.getTokens());
  DeclList* decls = parser.decls0();
  if (decls == nullptr) {
    llvm::errs() << parser.getError().render(srcCode.data(), locTab);
    return 1;
  }

  // type check
  Typer typer;
  typer.typeDeclList(decls);
  if (!typer.errors.empty()) {
    for (LocatedError err : typer.errors)
      llvm::errs() << err.render(srcCode.data(), locTab);
    return 1;
  }

  // borrow check
  if (!skipBorrowCheckingOpt) {
    BorrowChecker bc(typer.getTypeContext(), typer.ont);
    bc.checkDecls(decls);
    if (!bc.errors.empty()) {
      for (LocatedError err : bc.errors)
        llvm::errs() << err.render(srcCode.data(), locTab);
      return 1;
    }
  }

  // LLVM IR code generation
  Codegen codegen(typer.getTypeContext(), typer.ont);
  codegen.genDeclList(decls);
  codegen.mod->setModuleIdentifier(inFileOpt);
  codegen.mod->setSourceFileName(inFileOpt);

  // output LLVM IR to a file
  llvm::StringRef outFileStem = inFileOpt.getValue();
  size_t lastSlash = outFileStem.find_last_of('/');
  if (lastSlash != llvm::StringRef::npos)
    outFileStem = outFileStem.substr(lastSlash + 1);
  outFileStem.consume_back(".miscr");
  std::string llFile = emitLLVMOpt && !outFileOpt.empty() ?
    outFileOpt.getValue() : (outFileStem + ".ll").str();
  std::error_code EC;
  llvm::raw_fd_ostream outDotLL(llFile, EC);
  outDotLL << *codegen.mod;
  outDotLL.flush();
  decls->deleteRecursive();

  // invoke clang to compile the LLVM IR to a native binary
  if (!emitLLVMOpt) {
    pid_t childPID = fork();
    if (childPID == 0) {
      std::string outFile = outFileOpt.empty()
        ? outFileStem.str() : outFileOpt.getValue();
      execlp("clang", "clang", "-mllvm", "-opaque-pointers",
        "-o", outFile.c_str(), llFile.c_str(), nullptr);
      llvm::errs() << "Could not find clang. LLVM was output to "
                   << llFile.c_str() << "\n";
      return -1;
    } else {
      int status;
      wait(&status);
      if (status == 0) execlp("rm", "rm", llFile.c_str(), nullptr);
    }
  }

  return 0;
}