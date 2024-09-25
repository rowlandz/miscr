#include <vector>
#include "lexer/Lexer.hpp"
#include "parser/Parser.hpp"
#include "typer/Typer.hpp"
#include "test.hpp"

namespace TyperTests {
  TESTGROUP("Typer Tests")

  //////////////////////////////////////////////////////////////////////////////

  void expShouldHaveType(const char* expText, const char* expectedTy) {
    Lexer lexer(expText);
    if (!lexer.run()) throw std::runtime_error("Lexer error");
    Parser parser(lexer.getTokens());
    Exp* parsed = parser.exp();
    if (parsed == nullptr) throw std::runtime_error("Parser error");
    Typer typer;
    typer.typeExp(parsed);
    if (typer.unifier.getErrors()->size() > 0) {
      std::string errStr;
      for (auto err : *typer.unifier.getErrors())
        errStr.append(err.render(expText, lexer.getLocationTable()));
      throw std::runtime_error(errStr);
    }
    TVar infTy = parsed->getTVar();
    std::string infTyStr = typer.unifier.getTypeContext()->TVarToString(infTy);
    if (infTyStr != expectedTy) throw std::runtime_error(
      "Inferred " + infTyStr + " but expected " + expectedTy);
  }

  void expShouldFailTyper(const char* expText) {
    Lexer lexer(expText);
    if (!lexer.run()) throw std::runtime_error("Lexer error");
    Parser parser(lexer.getTokens());
    Exp* parsed = parser.exp();
    if (parsed == nullptr) throw std::runtime_error("Parser error");
    Typer typer;
    typer.typeExp(parsed);
    if (typer.unifier.getErrors()->size() == 0)
      throw std::runtime_error("Expected failure, but it succeeded.");
  }

  void declShouldPass(const char* declText) {
    Lexer lexer(declText);
    if (!lexer.run()) throw std::runtime_error("Lexer error");
    Parser parser(lexer.getTokens());
    Decl* parsed = parser.decl();
    if (parsed == nullptr) throw std::runtime_error("Parser error");
    Typer typer;
    typer.typeDecl(parsed);
    if (typer.unifier.getErrors()->size() > 0) {
      std::string errStr;
      for (auto err : *typer.unifier.getErrors())
        errStr.append(err.render(declText, lexer.getLocationTable()));
      throw std::runtime_error(errStr);
    }
  }

  void declShouldFail(const char* declText) {
    Lexer lexer(declText);
    if (!lexer.run()) throw std::runtime_error("Lexer error");
    Parser parser(lexer.getTokens());
    Decl* parsed = parser.decl();
    if (parsed == nullptr) throw std::runtime_error("Parser error");
    Typer typer;
    typer.typeDecl(parsed);
    if (typer.unifier.getErrors()->size() == 0)
      throw std::runtime_error("Expected failure, but it succeeded.");
  }

  //////////////////////////////////////////////////////////////////////////////

  TEST(primitive_literals) {
    expShouldHaveType("true", "bool");
    expShouldHaveType("false", "bool");
    expShouldHaveType("42", "numeric");
    expShouldHaveType("3.14", "decimal");
  }

  TEST(type_ascription) {
    expShouldHaveType("42: i32", "i32");
  }

  TEST(let_bindings) {
    expShouldHaveType("{ let x = 42; x; }", "numeric");
    expShouldHaveType("{ let x = 42; true; }", "bool");
    expShouldHaveType("{ let x = 42; }", "unit");
    expShouldHaveType("{ let x = 42; let y = x + 1; y; }", "numeric");
  }

  TEST(let_shadowing) {
    expShouldHaveType("{ let x = 42; let x = true; x; }", "bool");
  }

  TEST(references) {
    expShouldHaveType("&42", "rref<numeric>");
    expShouldHaveType("#42", "wref<numeric>");
  }

  TEST(deref_expression) {
    expShouldHaveType("(&0)!", "numeric");
    expShouldHaveType("(#0)!", "numeric");
  }

  TEST(store_expression) {
    expShouldHaveType("{ let x = #0; x := x! + 42 }", "numeric");
    expShouldFailTyper("{ let x = &0; x := 42; }");
  }

  TEST(wref_to_rref_coercion) {
    expShouldHaveType("(#42): &i32", "rref<i32>");
    expShouldFailTyper("(&42): #i32");
    expShouldHaveType(
      "{ let x = #0;"
      "  let y = x: &i32;"
      "  x := x! + 1;"
      "}",
      "i32"
    );
  }

  TEST(arrays_and_strings) {
    expShouldHaveType("[1,2,3]", "array_sact<3,numeric>");
    expShouldHaveType("[20 of 0]", "array_sart<???,numeric>");
    expShouldHaveType("\"hello\\n\"", "rref<array_sact<7,i8>>");
  }

  TEST(decls_and_call_expressions) {
    declShouldPass(
      "module Testing {"
      "  extern func f(x: i32): i32;"
      "  extern func p(y: i8): bool;"
      "  func g(x: i32): i32 = f(2*x) + 1;"
      "  func h(z: i8): i32 = if p(z) then 0 else 1;"
      "}"
    );
  }

  TEST(decls_with_references) {
    declShouldPass(
      "module Testing {"
      "  extern func f(x: &i32): unit;"
      "  func h(): unit = f(#42);"
      "}"
    );
    declShouldFail(
      "module Testing {"
      "  extern func f(x: #i32): unit;"
      "  func h(): unit = f(&42);"
      "}"
    );
  }

  TEST(indexing) {
    expShouldHaveType("(&[1,2,3])[0]", "rref<numeric>");
    expShouldHaveType("(#[1,2,3])[0]", "wref<numeric>");
  }
}