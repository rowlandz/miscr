#include <vector>
#include "lexer/Lexer.hpp"
#include "parser/Parser.hpp"
#include "typer/Typer.hpp"
#include "test.hpp"

namespace TyperTests {
  TESTGROUP("Typer Tests")

  //////////////////////////////////////////////////////////////////////////////

  void expShouldHaveType(const char* expText, NodeTy expectedTy) {
    Lexer lexer(expText);
    lexer.run();
    NodeManager m;
    Parser parser(&m, lexer.getTokens());
    auto parsed = parser.exp();
    Typer typer(&m);
    typer.typeExp(parsed);
    NodeTy observedTy = m.get(m.get(parsed).n1).ty;
    if (observedTy != expectedTy) throw std::runtime_error(
      "Expected type " + std::string(NodeTyToString(expectedTy)) +
      " but " + NodeTyToString(observedTy) + " was inferred."
    );
  }

  //////////////////////////////////////////////////////////////////////////////

  TEST(primitive_literals) {
    expShouldHaveType("true", NodeTy::BOOL);
    expShouldHaveType("false", NodeTy::BOOL);
    expShouldHaveType("42", NodeTy::NUMERIC);
    expShouldHaveType("3.14", NodeTy::DECIMAL);
    expShouldHaveType("\"hello\"", NodeTy::STRING);
  }

  TEST(type_ascription) {
    expShouldHaveType("42: i32", NodeTy::i32);
  }

  TEST(let_bindings) {
    expShouldHaveType("{ let x = 42; x; }", NodeTy::NUMERIC);
    expShouldHaveType("{ let x = 42; true; }", NodeTy::BOOL);
    expShouldHaveType("{ let x = 42; }", NodeTy::UNIT);
    expShouldHaveType("{ let x = 42; let y = x + 1; y; }", NodeTy::NUMERIC);
  }

  TEST(let_shadowing) {
    expShouldHaveType("{ let x = 42; let x = true; x; }", NodeTy::BOOL);
  }
}