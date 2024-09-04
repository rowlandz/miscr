#include <vector>
#include "lexer/Lexer.hpp"
#include "parser/Parser.hpp"
#include "test.hpp"

namespace ParserTests {
  TESTGROUP("Parser Tests")

  //////////////////////////////////////////////////////////////////////////////

  bool isAlphaNumU(char c) {
    return ('0' <= c && c <= '9')
        || ('A' <= c && c <= 'Z')
        || ('a' <= c && c <= 'z')
        || c == '_';
  }

  std::pair<int, NodeTy> parseLine(const char* line) {
    int indent = 0;
    while (*line != '\0' && !isAlphaNumU(*line)) { indent++; line++; }
    const char* beginToken = line;
    while (isAlphaNumU(*line)) { line++; }
    if (*line != '\0') throw std::runtime_error("Expected null char after token");
    NodeTy ty = stringToNodeTy(std::string(beginToken));
    return std::pair<int, NodeTy>(indent, ty);
  }

  /** Checks that the `lines` starting with `currentLine` forms a tree with
   * starting indentation `indent` that matches node `_n`. If successful,
   * `currentLine` is updated to point immediately after the detected tree. */
  void expectMatch(
    const NodeManager& m,
    unsigned int _n,
    int indent,
    std::vector<const char*>& lines,
    std::vector<const char*>::iterator& currentLine
  ) {
    if (currentLine >= lines.end()) throw std::runtime_error("Ran out of lines!");
    auto indAndTy = parseLine(*currentLine);
    currentLine++;
    if (indAndTy.first != indent) throw std::runtime_error("Unexpected indent");
    if (_n >= m.nodes.size()) throw std::runtime_error("Invalid node address");
    Node n = m.get(_n);
    if (n.ty != indAndTy.second) throw std::runtime_error("Node types did not match\n");

    if (n.n1 != NN) expectMatch(m, n.n1, indent+4, lines, currentLine); else return;
    if (n.n2 != NN) expectMatch(m, n.n2, indent+4, lines, currentLine); else return;
    if (n.n3 != NN) expectMatch(m, n.n3, indent+4, lines, currentLine); else return;
    if (n.extra.nodes.n4 != NN) expectMatch(m, n.extra.nodes.n4, indent+4, lines, currentLine); else return;
    if (n.extra.nodes.n5 != NN) expectMatch(m, n.extra.nodes.n5, indent+4, lines, currentLine); else return;
  }

  void expParseTreeShouldBe(const char* text, std::vector<const char*> expectedNodes) {
    Lexer lexer(text);
    lexer.run();
    NodeManager m;
    Parser parser(&m, lexer.getTokens());
    auto parsed = parser.exp();
    auto currentLine = expectedNodes.begin();
    expectMatch(m, parsed, 0, expectedNodes, currentLine);
  }

  void declParseTreeShouldBe(const char* text, std::vector<const char*> expectedNodes) {
    Lexer lexer(text);
    lexer.run();
    NodeManager m;
    Parser parser(&m, lexer.getTokens());
    auto parsed = parser.decl();
    auto currentLine = expectedNodes.begin();
    expectMatch(m, parsed, 0, expectedNodes, currentLine);
  }

  //////////////////////////////////////////////////////////////////////////////

  TEST(qident) {
    expParseTreeShouldBe("global::MyModule::myfunc", {
      "EQIDENT",
      "    TYPEVAR",
      "    QIDENT",
      "        IDENT",
      "        QIDENT",
      "            IDENT",
      "            IDENT",
    });
  }

  TEST(arithmetic) {
    expParseTreeShouldBe("1 + 1", {
      "ADD",
      "    TYPEVAR",
      "    LIT_INT",
      "        TYPEVAR",
      "    LIT_INT",
      "        TYPEVAR",
    });
  }

  TEST(block_expression) {
    expParseTreeShouldBe("{ let x = 10; x; }", {
      "BLOCK",
      "    TYPEVAR",
      "    STMTLIST_CONS",
      "        LET",
      "            IDENT",
      "            LIT_INT",
      "                TYPEVAR",
      "        STMTLIST_CONS",
      "            EQIDENT",
      "                TYPEVAR",
      "                IDENT",
      "            STMTLIST_NIL",
    });
  }

  TEST(main_prints_hello_world) {
    declParseTreeShouldBe("proc main(): i32 = { println(\"Hello World\"); };", {
      "PROC",
      "    IDENT",
      "    PARAMLIST_NIL",
      "    i32",
      "    BLOCK",
      "        TYPEVAR",
      "        STMTLIST_CONS",
      "            CALL",
      "                TYPEVAR",
      "                EQIDENT",
      "                    TYPEVAR",
      "                    IDENT",
      "                EXPLIST_CONS",
      "                    LIT_STRING",
      "                        TYPEVAR",
      "                    EXPLIST_NIL",
      "            STMTLIST_NIL",
    });
  }

  TEST(empty_module) {
    declParseTreeShouldBe("module M {}", {
      "MODULE",
      "    IDENT",
      "    DECLLIST_NIL",
    });
  }

  TEST(nested_decls) {
    declParseTreeShouldBe("module M { extern proc f(): unit; namespace N { extern proc g(): unit; } }", {
      "MODULE",
      "    IDENT",
      "    DECLLIST_CONS",
      "        EXTERN_PROC",
      "            IDENT",
      "            PARAMLIST_NIL",
      "            UNIT",
      "        DECLLIST_CONS",
      "            NAMESPACE",
      "                IDENT",
      "                DECLLIST_CONS",
      "                    EXTERN_PROC",
      "                        IDENT",
      "                        PARAMLIST_NIL",
      "                        UNIT",
      "                    DECLLIST_NIL",
      "            DECLLIST_NIL",
    });
  }

}