#include <vector>
#include "lexer/Lexer.hpp"
#include "parser/Parser.hpp"
#include "test.hpp"

namespace ParserTests {
  TESTGROUP("Parser Tests")

  //////////////////////////////////////////////////////////////////////////////

  class PT {
  public:
    NodeTy ty;
    std::vector<PT> subs;

    PT(NodeTy ty) {
      this->ty = ty;
    }

    PT(NodeTy ty, std::vector<PT> init) {
      this->ty = ty;
      subs = init;
    }
  };

  void parseTreesShouldMatch(const NodeManager &m, unsigned int _n, PT &expected) {
    if (_n >= m.nodes.size()) throw std::runtime_error("Invalid node address\n");
    Node n = m.get(_n);
    if (n.ty != expected.ty) throw std::runtime_error("Node types did not match\n");
    if (expected.subs.size() >= 1) parseTreesShouldMatch(m, n.n1, expected.subs[0]);
    if (expected.subs.size() >= 2) parseTreesShouldMatch(m, n.n2, expected.subs[1]);
    if (expected.subs.size() >= 3) parseTreesShouldMatch(m, n.n3, expected.subs[2]);
    if (expected.subs.size() >= 4) parseTreesShouldMatch(m, n.extra.nodes.n4, expected.subs[3]);
    if (expected.subs.size() == 5) parseTreesShouldMatch(m, n.extra.nodes.n5, expected.subs[4]);
    if (expected.subs.size() > 5) throw std::runtime_error("Expected node with more than 5 children\n");
  }

  void expParseTreeShouldBe(const char* text, PT expected) {
    auto tokens = Lexer().run(text);
    auto parser = Parser(tokens);
    auto parsed = parser.exp();
    parseTreesShouldMatch(parser.m, parsed, expected);
  }

  void declParseTreeShouldBe(const char* text, PT expected) {
    auto tokens = Lexer().run(text);
    auto parser = Parser(tokens);
    auto parsed = parser.funcOrProc();
    parseTreesShouldMatch(parser.m, parsed, expected);
  }

  // bool isAlphaNumU(char c) {
  //   return ('0' <= c && c <= '9')
  //       || ('A' <= c && c <= 'A')
  //       || ('a' <= c && c <= 'z')
  //       || c == '_';
  // }

  // std::pair<int, NodeTy> parseLine(const char* line) {
  //   int indent = 0;
  //   while (*line != '\0' && !isAlphaNumU(*line)) { indent++; line++; }
  //   const char* beginToken = line;
  //   while (isAlphaNumU(*line)) { line++; }
  //   if (*line != '\0') throw new std::runtime_error("Expected null char after token");
  //   NodeTy ty = stringToNodeTy(std::string(beginToken));
  //   return std::pair<int, NodeTy>(indent, ty);
  // }

  // void blahShouldMatch(int indent, std::vector<const char*>::iterator line,
  // const NodeManager& m, unsigned int _n) {
  //   auto indAndTy = parseLine(*(line++));
  //   if (indAndTy.first != indent) throw std::runtime_error("Unexpected indent\n");
  //   if (_n >= m.nodes.size()) throw std::runtime_error("Invalid node address\n");
  //   Node n = m.get(_n);
  //   if (n.ty != indAndTy.second) throw std::runtime_error("Node types did not match\n");

  //   indAndTy = parseLine(*line);
  //   if (expected.subs.size() >= 1) parseTreesShouldMatch(m, n.n1, expected.subs[0]);
  //   if (expected.subs.size() >= 2) parseTreesShouldMatch(m, n.n2, expected.subs[1]);
  //   if (expected.subs.size() >= 3) parseTreesShouldMatch(m, n.n3, expected.subs[2]);
  //   if (expected.subs.size() >= 4) parseTreesShouldMatch(m, n.extra.nodes.n4, expected.subs[3]);
  //   if (expected.subs.size() == 5) parseTreesShouldMatch(m, n.extra.nodes.n5, expected.subs[4]);
  //   if (expected.subs.size() > 5) throw std::runtime_error("Expected node with more than 5 children\n");
    
  // }

  //////////////////////////////////////////////////////////////////////////////

  TEST(arithmetic) {
    expParseTreeShouldBe("1 + 1",
      PT(NodeTy::ADD, {
        PT(NodeTy::TYPEVAR),
        PT(NodeTy::LIT_INT),
        PT(NodeTy::LIT_INT)
      })
    );
  }

  TEST(block_expression) {
    expParseTreeShouldBe("{ let x = 10; x + 1; }",
      PT(NodeTy::BLOCK, {
        PT(NodeTy::TYPEVAR),
        PT(NodeTy::STMTLIST_CONS, {
          PT(NodeTy::LET, {
            PT(NodeTy::IDENT),
            PT(NodeTy::LIT_INT, {
              PT(NodeTy::TYPEVAR)
            })
          }),
          PT(NodeTy::STMTLIST_CONS, {
            PT(NodeTy::ADD, {
              PT(NodeTy::TYPEVAR),
              PT(NodeTy::EIDENT, {
                PT(NodeTy::TYPEVAR)
              }),
              PT(NodeTy::LIT_INT, {
                PT(NodeTy::TYPEVAR)
              })
            }),
            PT(NodeTy::STMTLIST_NIL)
          })
        })
      })
    );
  }

  TEST(main_prints_hello_world) {
    declParseTreeShouldBe("proc main(): i32 = { println(\"Hello World\"); };",
      PT(NodeTy::PROC, {
        PT(NodeTy::IDENT),
        PT(NodeTy::PARAMLIST_NIL),
        PT(NodeTy::i32),
        PT(NodeTy::BLOCK, {
          PT(NodeTy::TYPEVAR),
          PT(NodeTy::STMTLIST_CONS, {
            PT(NodeTy::CALL, {
              PT(NodeTy::TYPEVAR),
              PT(NodeTy::EIDENT, {
                PT(NodeTy::TYPEVAR)
              }),
              PT(NodeTy::EXPLIST_CONS, {
                PT(NodeTy::LIT_STRING, {
                  PT(NodeTy::TYPEVAR)
                }),
                PT(NodeTy::EXPLIST_NIL)
              })
            }),
            PT(NodeTy::STMTLIST_NIL)
          })
        }),
      })
    );
  }

}