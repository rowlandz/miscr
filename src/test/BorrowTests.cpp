#include <vector>
#include "lexer/Lexer.hpp"
#include "parser/Parser.hpp"
#include "typer/Typer.hpp"
#include "borrowchecker/BorrowChecker.hpp"
#include "test.hpp"

namespace BorrowTests {
  TESTGROUP("Borrow Checker Tests")

  //==========================================================================//

  void declsShouldPass(const char* declsText) {
    Lexer lexer(declsText);
    if (!lexer.run()) throw std::runtime_error("Lexer error");
    Parser parser(lexer.getTokens());
    DeclList* parsed = parser.decls0();
    if (parsed == nullptr) throw std::runtime_error("Parser error");
    Typer typer;
    typer.typeDeclList(parsed);
    if (!typer.errors.empty()) {
      std::string errStr;
      for (auto err : typer.errors)
        errStr.append(err.render(declsText, lexer.getLocationTable()));
      throw std::runtime_error(errStr);
    }
    BorrowChecker bc(typer.getTypeContext(), typer.ont);
    bc.checkDecls(parsed);
    if (!bc.errors.empty()) {
      std::string errStr;
      for (auto err : bc.errors)
        errStr.append(err.render(declsText, lexer.getLocationTable()));
      throw std::runtime_error(errStr);
    }
  }

  void declsShouldFail(const char* declsText) {
    Lexer lexer(declsText);
    if (!lexer.run()) throw std::runtime_error("Lexer error");
    Parser parser(lexer.getTokens());
    DeclList* parsed = parser.decls0();
    if (parsed == nullptr) throw std::runtime_error("Parser error");
    Typer typer;
    typer.typeDeclList(parsed);
    if (!typer.errors.empty()) {
      std::string errStr;
      for (auto err : typer.errors)
        errStr.append(err.render(declsText, lexer.getLocationTable()));
      throw std::runtime_error(errStr);
    }
    BorrowChecker bc(typer.getTypeContext(), typer.ont);
    bc.checkDecls(parsed);
    if (bc.errors.empty()) {
      throw std::runtime_error("Expected borrow checking to fail.");
    }
  }

  //==========================================================================//

  TEST(malloc_then_free) {
    declsShouldPass(
      "extern func malloc(size: i64): #i8;\n"
      "extern func free(ptr: #i8): unit;\n"
      "func foo(): unit = {\n"
      "  let x = malloc(10);\n"
      "  free(x);\n"
      "};"
    );
  }

  TEST(unfreed_oref) {
    declsShouldFail(
      "extern func malloc(size: i64): #i8;\n"
      "extern func free(ptr: #i8): unit;\n"
      "func foo(): unit = {\n"
      "  let x = malloc(10);\n"
      "};"
    );
  }

  TEST(double_freed_oref) {
    declsShouldFail(
      "extern func malloc(size: i64): #i8;\n"
      "extern func free(ptr: #i8): unit;\n"
      "func foo(): unit = {\n"
      "  let x = malloc(10);\n"
      "  free(x);\n"
      "  free(x);\n"
      "};"
    );
  }

  TEST(immediately_borrowed_malloc) {
    declsShouldFail(
      "extern func malloc(size: i64): #i8;\n"
      "func foo(): &i8 = borrow malloc(10);"
    );
  }


}