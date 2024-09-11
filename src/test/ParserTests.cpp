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

  std::pair<int, AST::ID> parseLine(const char* line) {
    int indent = 0;
    while (*line != '\0' && !isAlphaNumU(*line)) { indent++; line++; }
    const char* beginToken = line;
    while (isAlphaNumU(*line)) { line++; }
    if (*line != '\0') throw std::runtime_error("Expected null char after token");
    AST::ID ty = stringToASTID(std::string(beginToken));
    return std::pair<int, AST::ID>(indent, ty);
  }

  /** Checks that the `lines` starting with `currentLine` forms a tree with
   * starting indentation `indent` that matches node `_n`. If successful,
   * `currentLine` is updated to point immediately after the detected tree. */
  void expectMatch(
    const ASTContext& ctx,
    Addr<AST> _n,
    int indent,
    std::vector<const char*>& lines,
    int* currentLine
  ) {
    if (*currentLine >= lines.size()) throw std::runtime_error("Ran out of lines!");
    auto indAndTy = parseLine(lines[*currentLine]);
    if (indAndTy.first != indent) throw std::runtime_error("Unexpected indent");
    if (!ctx.isValid(_n)) throw std::runtime_error("Invalid node address");
    AST n = ctx.get(_n);
    if (n.getID() != indAndTy.second) {
      std::string errMsg("Node types did not match on line ");
      errMsg.append(std::to_string(*currentLine));
      errMsg.append(". Expected ");
      errMsg.append(ASTIDToString(indAndTy.second));
      errMsg.append(" but got ");
      errMsg.append(ASTIDToString(n.getID()));
      errMsg.append(".\n");
      throw std::runtime_error(errMsg);
    }
    *currentLine = *currentLine + 1;

    std::vector<Addr<AST>> subnodes = getSubnodes(ctx, _n);
    for (Addr<AST> subnode : subnodes) {
      expectMatch(ctx, subnode, indent+4, lines, currentLine);
    }
  }

  void expParseTreeShouldBe(const char* text, std::vector<const char*> expectedNodes) {
    Lexer lexer(text);
    lexer.run();
    ASTContext ctx;
    Parser parser(&ctx, lexer.getTokens());
    auto parsed = parser.exp();
    if (parsed.isError()) throw std::runtime_error(parser.getError().render(text, lexer.getLocationTable()));
    int currentLine = 0;
    expectMatch(ctx, parsed.upcast<AST>(), 0, expectedNodes, &currentLine);
  }

  void declParseTreeShouldBe(const char* text, std::vector<const char*> expectedNodes) {
    Lexer lexer(text);
    lexer.run();
    ASTContext ctx;
    Parser parser(&ctx, lexer.getTokens());
    auto parsed = parser.decl();
    if (parsed.isError()) throw std::runtime_error(parser.getError().render(text, lexer.getLocationTable()));
    int currentLine = 0;
    expectMatch(ctx, parsed.upcast<AST>(), 0, expectedNodes, &currentLine);
  }

  //////////////////////////////////////////////////////////////////////////////

  TEST(qident) {
    expParseTreeShouldBe("global::MyModule::myfunc", {
      "ENAME",
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
      "    INT_LIT",
      "    INT_LIT",
    });
  }

  TEST(block_expression) {
    expParseTreeShouldBe("{ let x = 10; x; }", {
      "BLOCK",
      "    EXPLIST_CONS",
      "        LET",
      "            IDENT",
      "            INT_LIT",
      "        EXPLIST_CONS",
      "            ENAME",
      "                IDENT",
      "            EXPLIST_NIL",
    });
  }

  // TEST(array_constructor_list) {
  //   expParseTreeShouldBe("[42, x, y+1]", {
  //     "ARRAY_CONSTR_LIST",
  //     "    TYPEVAR",
  //     "    EXPLIST_CONS",
  //     "        INT_LIT",
  //     "            TYPEVAR",
  //     "        EXPLIST_CONS",
  //     "            EQIDENT",
  //     "                TYPEVAR",
  //     "                IDENT",
  //     "            EXPLIST_CONS",
  //     "                ADD",
  //     "                    TYPEVAR",
  //     "                    EQIDENT",
  //     "                        TYPEVAR",
  //     "                        IDENT",
  //     "                    INT_LIT",
  //     "                        TYPEVAR",
  //     "                EXPLIST_NIL",
  //   });
  // }

  // TEST(array_constructor_init) {
  //   expParseTreeShouldBe("[20 of 0]", {
  //     "ARRAY_CONSTR_INIT",
  //     "    TYPEVAR",
  //     "    INT_LIT",
  //     "        TYPEVAR",
  //     "    INT_LIT",
  //     "        TYPEVAR",
  //   });
  // }

  TEST(main_prints_hello_world) {
    declParseTreeShouldBe("func main(): i32 = { println(\"Hello World\"); };", {
      "FUNC",
      "    IDENT",
      "    PARAMLIST_NIL",
      "    i32_TEXP",
      "    BLOCK",
      "        EXPLIST_CONS",
      "            CALL",
      "                IDENT",
      "                EXPLIST_CONS",
      "                    STRING_LIT",
      "                    EXPLIST_NIL",
      "            EXPLIST_NIL",
    });
  }

  TEST(empty_module) {
    declParseTreeShouldBe("module M {}", {
      "MODULE",
      "    IDENT",
      "    DECLLIST_NIL",
    });
  }

  // TEST(nested_decls) {
  //   declParseTreeShouldBe(
  //     "module M {\n"
  //     "  extern func f(): unit;\n"
  //     "  namespace N {\n"
  //     "    extern func g(): unit;\n"
  //     "  }\n"
  //     "}\n"
  //   , {
  //     "MODULE",
  //     "    IDENT",
  //     "    DECLLIST_CONS",
  //     "        EXTERN_PROC",
  //     "            IDENT",
  //     "            PARAMLIST_NIL",
  //     "            UNIT",
  //     "        DECLLIST_CONS",
  //     "            NAMESPACE",
  //     "                IDENT",
  //     "                DECLLIST_CONS",
  //     "                    EXTERN_PROC",
  //     "                        IDENT",
  //     "                        PARAMLIST_NIL",
  //     "                        UNIT",
  //     "                    DECLLIST_NIL",
  //     "            DECLLIST_NIL",
  //   });
  // }

}