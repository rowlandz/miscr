#include "lexer/Lexer.hpp"
#include "parser/Parser.hpp"
#include "typer/Typer.hpp"
#include "borrowchecker/BorrowChecker.hpp"
#include "test.hpp"

namespace BorrowTests {
  TESTGROUP("Borrow Checker Tests")

  //==========================================================================//

  std::optional<std::string> declsShouldPass(const char* declsText) {
    Lexer lexer(declsText);
    if (!lexer.run()) return "Lexer error";
    Parser parser(lexer.getTokens());
    DeclList* parsed = parser.decls0();
    if (parsed == nullptr) return "Parser error";
    Typer typer;
    typer.typeDeclList(parsed);
    if (typer.hasErrors()) {
      std::string errStr;
      for (auto err : typer.getErrors())
        errStr.append(err.render(declsText, lexer.getLocationTable()));
      return (errStr);
    }
    BorrowChecker bc(typer.getTypeContext(), typer.getOntology());
    bc.checkDecls(parsed);
    if (!bc.errors.empty()) {
      std::string errStr;
      for (auto err : bc.errors)
        errStr.append(err.render(declsText, lexer.getLocationTable()));
      return (errStr);
    }
    SUCCESS
  }

  std::optional<std::string> declsShouldFail(const char* declsText) {
    Lexer lexer(declsText);
    if (!lexer.run()) return "Lexer error";
    Parser parser(lexer.getTokens());
    DeclList* parsed = parser.decls0();
    if (parsed == nullptr) return "Parser error";
    Typer typer;
    typer.typeDeclList(parsed);
    if (typer.hasErrors()) {
      std::string errStr;
      for (auto err : typer.getErrors())
        errStr.append(err.render(declsText, lexer.getLocationTable()));
      return (errStr);
    }
    BorrowChecker bc(typer.getTypeContext(), typer.getOntology());
    bc.checkDecls(parsed);
    if (bc.errors.empty()) {
      return "Expected borrow checking to fail.";
    }
    SUCCESS
  }

  //==========================================================================//

  TEST(malloc_then_free) {
    return declsShouldPass(
      "extern func malloc(size: i64): #i8;\n"
      "extern func free(ptr: #i8): unit;\n"
      "func foo(): unit = {\n"
      "  let x = malloc(10);\n"
      "  free(x);\n"
      "};"
    );
  }

  TEST(unfreed_oref) {
    return declsShouldFail(
      "extern func malloc(size: i64): #i8;\n"
      "extern func free(ptr: #i8): unit;\n"
      "func foo(): unit = {\n"
      "  let x = malloc(10);\n"
      "};"
    );
  }

  TEST(double_freed_oref) {
    return declsShouldFail(
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
    return declsShouldFail(
      "extern func malloc(size: i64): #i8;\n"
      "func foo(): &i8 = borrow malloc(10);"
    );
  }

  TEST(ref_then_deref) {
    return declsShouldPass(
      "extern func alloc(): #i8;\n"
      "extern func free(ptr: #i8): unit;\n"
      "func foo(): unit = {\n"
      "  let x = &&alloc();\n"
      "  let y = x!;\n"
      "  free(y!);\n"
      "};"
    );
  }

  TEST(double_free_with_derefs) {
    return declsShouldFail(
      "extern func alloc(): #i8;\n"
      "extern func free2(p1: #i8, p2: #i8): unit;\n"
      "func foo(): unit = {\n"
      "  let x = &&alloc();\n"
      "  let y = x!;\n"
      "  free2(y!, x!!);\n"
      "};"
    );
  }

  TEST(let_is_not_a_use) {
    return declsShouldPass(
      "func foo(x: #i8): #i8 = {\n"
      "  let y = x;\n"
      "  x\n"
      "};"
    );
  }

  TEST(ref_is_not_a_use) {
    TRY(declsShouldFail("func foo(x: #i8): &#i8 = &x;"));
    TRY(declsShouldPass(
      "func foo(x: #i8): #i8 = {\n"
      "  let y = &x;\n"
      "  let z = &x;\n"
      "  z!"
      "};"
    ));
    SUCCESS
  }

  TEST(sneaky_proj_deref_double_use) {
    return declsShouldFail(
      "data Thing(fst: #i8)\n"
      "extern func alloc(): #i8;\n"
      "extern func free(ptr: #i8): unit;\n"
      "func foo(): unit = {\n"
      "  let p = &Thing(alloc());\n"
      "  free(p!.fst);\n"
      "  free(p[.fst]!);\n"
      "};"
    );
  }

  TEST(simple_move_and_replace) {
    return declsShouldPass(
      "extern func alloc(): #i8;\n"
      "extern func free(ptr: #i8): unit;\n"
      "func foo(x: &#i8): unit = {\n"
      "  free(move x!);\n"
      "  x := alloc();\n"
      "};"
    );
  }

  TEST(unreplaced_move) {
    return declsShouldFail(
      "extern func free(ptr: #i8): unit;\n"
      "func foo(x: &#i8): unit = free(move x!);"
    );
  }

  TEST(overwriting_oref) {
    return declsShouldFail("func foo(x: &#i8, y: #i8): unit = { x := y };");
  }

  TEST(if_expr_inconsistent_frees) {
    return declsShouldFail(
      "extern func free(ptr: #i8): unit;\n"
      "func foo(x: #i8, c: bool): unit = if (c) free(x) else {};\n"
    );
  }

}