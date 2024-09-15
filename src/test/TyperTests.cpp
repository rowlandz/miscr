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
    ASTContext ctx;
    Parser parser(&ctx, lexer.getTokens());
    Addr<Exp> parsed = parser.exp();
    if (parsed.isError()) throw std::runtime_error("Parser error");
    Typer typer(&ctx);
    typer.typeExp(parsed);
    if (typer.unifier.errors.size() > 0) {
      std::string errStr;
      for (auto err : typer.unifier.errors)
        errStr.append(err.render(expText, lexer.getLocationTable()));
      throw std::runtime_error(errStr);
    }
    TVar infTy = ctx.get(parsed).getTVar();
    std::string infTyStr = typer.unifier.getTypeContext()->TVarToString(infTy);
    if (infTyStr != expectedTy) throw std::runtime_error(
      "Inferred " + infTyStr + " but expected " + expectedTy);
  }

  void expShouldFailTyper(const char* expText) {
    Lexer lexer(expText);
    if (!lexer.run()) throw std::runtime_error("Lexer error");
    ASTContext ctx;
    Parser parser(&ctx, lexer.getTokens());
    Addr<Exp> parsed = parser.exp();
    if (parsed.isError()) throw std::runtime_error("Parser error");
    Typer typer(&ctx);
    typer.typeExp(parsed);
    if (typer.unifier.errors.size() == 0)
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
}