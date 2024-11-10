#include "lexer/Lexer.hpp"
#include "parser/Parser.hpp"
#include "test.hpp"

namespace ParserTests {
  TESTGROUP("Parser Tests")

  //==========================================================================//

  bool isAlphaNumU(char c) {
    return ('0' <= c && c <= '9')
        || ('A' <= c && c <= 'Z')
        || ('a' <= c && c <= 'z')
        || c == '_';
  }

  std::pair<int, AST::ID> parseLine(const char* line) {
    int indent = 0;
    while (*line != '\0' && !isAlphaNumU(*line)) { indent++; line++; }
    const char* beginToken = line;
    while (isAlphaNumU(*line)) { line++; }
    if (*line != '\0') throw std::runtime_error("Expected null char after token");
    AST::ID ty = stringToASTID(std::string(beginToken));
    return std::pair<int, AST::ID>(indent, ty);
  }

  /// Checks that the `lines` starting with `currentLine` forms a tree with
  /// starting indentation `indent` that matches node `_n`. If successful,
  /// `currentLine` is updated to point immediately after the detected tree.
  void expectMatch(
    AST* n,
    int indent,
    std::vector<const char*>& lines,
    int* currentLine
  ) {
    if (*currentLine >= lines.size()) throw std::runtime_error("Ran out of lines!");
    auto indAndTy = parseLine(lines[*currentLine]);
    if (indAndTy.first != indent) throw std::runtime_error("Unexpected indent");
    if (n == nullptr) throw std::runtime_error("Invalid node address");
    if (n->id != indAndTy.second) {
      std::string errMsg("Node types did not match on line ");
      errMsg.append(std::to_string(*currentLine));
      errMsg.append(". Expected ");
      errMsg.append(AST::IDToString(indAndTy.second));
      errMsg.append(" but got ");
      errMsg.append(AST::IDToString(n->id));
      errMsg.append(".\n");
      throw std::runtime_error(errMsg);
    }
    *currentLine = *currentLine + 1;

    for (AST* subnode : n->getASTChildren())
      expectMatch(subnode, indent+4, lines, currentLine);
  }

  void expParseTreeShouldBe(const char* text, std::vector<const char*> expectedNodes) {
    Lexer lexer(text);
    lexer.run();
    Parser parser(lexer.getTokens());
    auto parsed = parser.exp();
    if (parsed == nullptr) throw std::runtime_error(parser.getError().render(text, lexer.getLocationTable()));
    int currentLine = 0;
    expectMatch(parsed, 0, expectedNodes, &currentLine);
  }

  void declParseTreeShouldBe(const char* text, std::vector<const char*> expectedNodes) {
    Lexer lexer(text);
    lexer.run();
    Parser parser(lexer.getTokens());
    auto parsed = parser.decl();
    if (parsed == nullptr) throw std::runtime_error(parser.getError().render(text, lexer.getLocationTable()));
    int currentLine = 0;
    expectMatch(parsed, 0, expectedNodes, &currentLine);
  }

  //==========================================================================//

  TEST(qident) {
    expParseTreeShouldBe("global::MyModule::myfunc", {
      "ENAME",
      "    NAME"
    });
  }

  TEST(arithmetic) {
    expParseTreeShouldBe("1 + 1", {
      "BINOP_EXP",
      "    INT_LIT",
      "    INT_LIT",
    });
  }

  TEST(logical_binop_precedence) {
    expParseTreeShouldBe("1 && 2 || 3 && 4", {
      "BINOP_EXP",
      "    BINOP_EXP",
      "        INT_LIT",
      "        INT_LIT",
      "    BINOP_EXP",
      "        INT_LIT",
      "        INT_LIT",
    });
  }

  TEST(block_expression) {
    expParseTreeShouldBe("{ let x = 10; x; }", {
      "BLOCK",
      "    EXPLIST",
      "        LET",
      "            NAME",
      "            INT_LIT",
      "        ENAME",
      "            NAME",
    });
  }

  TEST(main_prints_hello_world) {
    declParseTreeShouldBe("func main(): i32 = { println(\"Hello World\"); };", {
      "FUNC",
      "    NAME",
      "    PARAMLIST",
      "    PRIMITIVE_TEXP",
      "    BLOCK",
      "        EXPLIST",
      "            CALL",
      "                NAME",
      "                EXPLIST",
      "                    STRING_LIT",
    });
  }

  TEST(empty_module) {
    declParseTreeShouldBe("module M {}", {
      "MODULE",
      "    NAME",
      "    DECLLIST",
    });
  }

  TEST(nested_decls) {
    declParseTreeShouldBe(
      "module M {\n"
      "  extern func f(): unit;\n"
      "  module N {\n"
      "    extern func g(): unit;\n"
      "  }\n"
      "}\n"
    , {
      "MODULE",
      "    NAME",
      "    DECLLIST",
      "        FUNC",
      "            NAME",
      "            PARAMLIST",
      "            PRIMITIVE_TEXP",
      "        MODULE",
      "            NAME",
      "            DECLLIST",
      "                FUNC",
      "                    NAME",
      "                    PARAMLIST",
      "                    PRIMITIVE_TEXP",
    });
  }

}