#include <vector>
#include "lexer/Lexer.hpp"
#include "parser/Parser.hpp"
#include "typer/Typer.hpp"
#include "test.hpp"

namespace TyperTests {
  TESTGROUP("Typer Tests")

  //==========================================================================//

  void expShouldHaveType(const char* expText, const char* expectedTy) {
    Lexer lexer(expText);
    if (!lexer.run()) throw std::runtime_error("Lexer error");
    Parser parser(lexer.getTokens());
    Exp* parsed = parser.exp();
    if (parsed == nullptr) throw std::runtime_error("Parser error");
    Typer typer;
    typer.typeExp(parsed);
    if (typer.hasErrors()) {
      std::string errStr;
      for (auto err : typer.getErrors())
        errStr.append(err.render(expText, lexer.getLocationTable()));
      throw std::runtime_error(errStr);
    }
    TVar infTy = parsed->getTVar();
    std::string infTyStr = typer.getTypeContext().TVarToString(infTy);
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
    if (typer.hasNoErrors())
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
    if (typer.hasErrors()) {
      std::string errStr;
      for (auto err : typer.getErrors())
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
    if (typer.hasNoErrors())
      throw std::runtime_error("Expected failure, but it succeeded.");
  }

  //==========================================================================//

  TEST(types_of_literals) {
    expShouldHaveType("true", "bool");
    expShouldHaveType("false", "bool");
    expShouldHaveType("42", "numeric");
    expShouldHaveType("3.14", "decimal");
    expShouldHaveType("\"hello\\n\"", "bref<i8>");
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

  TEST(unbound_identifier) {
    expShouldFailTyper("foobar");
  }

  TEST(references) {
    expShouldHaveType("&42", "bref<numeric>");
  }

  TEST(deref_expression) {
    expShouldHaveType("(&0)!", "numeric");
  }

  TEST(store_expression) {
    expShouldHaveType("{ let x = &0; x := x! + 42 }", "unit");
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
      "  func h(): unit = f(&42);"
      "}"
    );
  }

  TEST(indexing) {
    expShouldHaveType("(\"hello\")[0]", "bref<i8>");
  }

  TEST(datatypes_and_field_access) {
    declShouldPass(
      "module Testing {"
      "  data Person(name: &i8, age: i8)"
      "  func blah(p: &Person): unit = {"
      "    let n1: &&i8 = p[.name];"
      "    let n2: &i8  = p!.name;"
      "    let n3: &i8  = p->name;"
      "  };"
      "}"
    );
  }

  TEST(variadic_function) {
    declShouldPass(
      "module Testing {"
      "  extern func foo(x: i32, y: &i8, ...): i32;"
      "  func bar(): i32 = foo(0, \"hi\", true, 42);"
      "}"
    );
  }

}