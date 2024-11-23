#include <vector>
#include "lexer/Lexer.hpp"
#include "parser/Parser.hpp"
#include "typer/Typer.hpp"
#include "test.hpp"

namespace TyperTests {
  TESTGROUP("Typer Tests")

  //==========================================================================//

  std::optional<std::string>
  expShouldHaveType(const char* expText, const char* expectedTy) {
    Lexer lexer(expText);
    if (!lexer.run()) return "Lexer error";
    Parser parser(lexer.getTokens());
    Exp* parsed = parser.exp();
    if (parsed == nullptr) "Parser error";
    Typer typer;
    typer.typeExp(parsed);
    if (typer.hasErrors()) {
      std::string errStr;
      for (auto err : typer.getErrors())
        errStr.append(err.render(expText, lexer.getLocationTable()));
      return errStr;
    }
    std::string infTyStr = parsed->getType()->asString();
    if (infTyStr != expectedTy)
      return "Inferred " + infTyStr + " but expected " + expectedTy;
    SUCCESS
  }

  std::optional<std::string> expShouldFailTyper(const char* expText) {
    Lexer lexer(expText);
    if (!lexer.run()) return "Lexer error";
    Parser parser(lexer.getTokens());
    Exp* parsed = parser.exp();
    if (parsed == nullptr) return "Parser error";
    Typer typer;
    typer.typeExp(parsed);
    if (typer.hasNoErrors())
      return "Expected failure, but it succeeded.";
    SUCCESS
  }

  std::optional<std::string> declShouldPass(const char* declText) {
    Lexer lexer(declText);
    if (!lexer.run()) return "Lexer error";
    Parser parser(lexer.getTokens());
    Decl* parsed = parser.decl();
    if (parsed == nullptr) return "Parser error";
    Typer typer;
    typer.typeDecl(parsed);
    if (typer.hasErrors()) {
      std::string errStr;
      for (auto err : typer.getErrors())
        errStr.append(err.render(declText, lexer.getLocationTable()));
      return errStr;
    }
    SUCCESS
  }

  std::optional<std::string> declShouldFail(const char* declText) {
    Lexer lexer(declText);
    if (!lexer.run()) return "Lexer error";
    Parser parser(lexer.getTokens());
    Decl* parsed = parser.decl();
    if (parsed == nullptr) return "Parser error";
    Typer typer;
    typer.typeDecl(parsed);
    if (typer.hasNoErrors())
      return "Expected failure, but it succeeded.";
    SUCCESS
  }

  //==========================================================================//

  TEST(types_of_literals) {
    TRY(expShouldHaveType("true", "bool"));
    TRY(expShouldHaveType("false", "bool"));
    TRY(expShouldHaveType("42", "numeric"));
    TRY(expShouldHaveType("3.14", "decimal"));
    TRY(expShouldHaveType("\"hello\\n\"", "&i8"));
    SUCCESS
  }

  TEST(type_ascription) {
    return expShouldHaveType("42: i32", "i32");
  }

  TEST(let_bindings) {
    TRY(expShouldHaveType("{ let x = 42; x; }", "numeric"));
    TRY(expShouldHaveType("{ let x = 42; true; }", "bool"))
    TRY(expShouldHaveType("{ let x = 42; }", "unit"))
    TRY(expShouldHaveType("{ let x = 42; let y = x + 1; y; }", "numeric"))
    SUCCESS
  }

  TEST(let_shadowing) {
    return expShouldHaveType("{ let x = 42; let x = true; x; }", "bool");
  }

  TEST(unbound_identifier) {
    return expShouldFailTyper("foobar");
  }

  TEST(references) {
    return expShouldHaveType("&42", "&numeric");
  }

  TEST(deref_expression) {
    return expShouldHaveType("(&0)!", "numeric");
  }

  TEST(store_expression) {
    return expShouldHaveType("{ let x = &0; x := x! + 42 }", "unit");
  }

  TEST(decls_and_call_expressions) {
    return declShouldPass(
      "module Testing {"
      "  extern func f(x: i32): i32;"
      "  extern func p(y: i8): bool;"
      "  func g(x: i32): i32 = f(2*x) + 1;"
      "  func h(z: i8): i32 = if (p(z)) 0 else 1;"
      "}"
    );
  }

  TEST(decls_with_references) {
    return declShouldPass(
      "module Testing {"
      "  extern func f(x: &i32): unit;"
      "  func h(): unit = f(&42);"
      "}"
    );
  }

  TEST(indexing) {
    return expShouldHaveType("(\"hello\")[0]", "&i8");
  }

  TEST(datatypes_and_field_access) {
    return declShouldPass(
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
    return declShouldPass(
      "module Testing {"
      "  extern func foo(x: i32, y: &i8, ...): i32;"
      "  func bar(): i32 = foo(0, \"hi\", true, 42);"
      "}"
    );
  }

}