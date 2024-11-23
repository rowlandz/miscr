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

  std::optional<std::string>
  parseLine(const char* line, int& indent, AST::ID& parsedID) {
    indent = 0;
    while (*line != '\0' && !isAlphaNumU(*line)) { ++indent; ++line; }
    const char* beginToken = line;
    while (isAlphaNumU(*line)) { ++line; }
    if (*line != '\0') return "Expected null char after token";
    parsedID = stringToASTID(std::string(beginToken));
    SUCCESS
  }

  /// Checks that the `lines` starting with `currentLine` forms a tree with
  /// starting indentation `indent` that matches node `_n`. If successful,
  /// `currentLine` is updated to point immediately after the detected tree.
  std::optional<std::string> expectMatch(
    AST* n,
    int indent,
    std::vector<const char*>& lines,
    int* currentLine
  ) {
    if (*currentLine >= lines.size()) return "Ran out of lines!";
    int expectedIndent;
    AST::ID expectedID;
    TRY(parseLine(lines[*currentLine], expectedIndent, expectedID));
    if (expectedIndent != indent) return "Unexpected indent";
    if (n == nullptr) return "Invalid node address";
    if (n->id != expectedID) {
      return "Node types did not match on line " + std::to_string(*currentLine)
           + ". Expected " + AST::IDToString(expectedID) + " but got "
           + AST::IDToString(n->id) + ".\n";
    }
    *currentLine = *currentLine + 1;

    for (AST* subnode : n->getASTChildren()) {
      if (auto error = expectMatch(subnode, indent+4, lines, currentLine))
        return error;
    }

    SUCCESS
  }

  std::optional<std::string>
  expParseTreeShouldBe(const char* text, std::vector<const char*> expected) {
    Lexer lexer(text);
    lexer.run();
    Parser parser(lexer.getTokens());
    Exp* parsed = parser.exp();
    if (parsed == nullptr)
      return parser.getError().render(text, lexer.getLocationTable());
    int currentLine = 0;
    return expectMatch(parsed, 0, expected, &currentLine);
  }

  std::optional<std::string>
  declParseTreeShouldBe(const char* text, std::vector<const char*> expected) {
    Lexer lexer(text);
    lexer.run();
    Parser parser(lexer.getTokens());
    Decl* parsed = parser.decl();
    if (parsed == nullptr)
      return parser.getError().render(text, lexer.getLocationTable());
    int currentLine = 0;
    return expectMatch(parsed, 0, expected, &currentLine);
  }

  //==========================================================================//

  TEST(qident) {
    return expParseTreeShouldBe("global::MyModule::myfunc", {
      "ENAME",
      "    NAME"
    });
  }

  TEST(arithmetic) {
    return expParseTreeShouldBe("1 + 1", {
      "BINOP_EXP",
      "    INT_LIT",
      "    INT_LIT",
    });
  }

  TEST(logical_binop_precedence) {
    return expParseTreeShouldBe("1 && 2 || 3 && 4", {
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
    return expParseTreeShouldBe("{ let x = 10; x; }", {
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
    return declParseTreeShouldBe(
      "func main(): i32 = { println(\"Hello World\"); };"
    , {
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
    return declParseTreeShouldBe("module M {}", {
      "MODULE",
      "    NAME",
      "    DECLLIST",
    });
  }

  TEST(nested_decls) {
    return declParseTreeShouldBe(
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