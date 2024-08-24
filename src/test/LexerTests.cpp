#include <memory>
#include <cstring>
#include "lexer/Lexer.hpp"
#include "test.hpp"

namespace LexerTests {
  TESTGROUP("Lexer Tests")

  /** Succeeds if `text` is lexed into `expected`. */
  void tokensShouldBe(const char* text, std::vector<TokenTy> expected);

  TEST(simple_example_1) {
    tokensShouldBe("(1 + 2) * 3", {
      LPAREN, LIT_INT, OP_ADD, LIT_INT, RPAREN, OP_MUL, LIT_INT, END
    });
  }

  TEST(keywords_and_identifiers) {
    tokensShouldBe("func funcy let", {
      KW_FUNC, TOK_IDENT, KW_LET, END
    });
  }

  TEST(operators) {
    tokensShouldBe("=  =>  ==  /=", {
      EQUAL, FATARROW, OP_EQ, OP_NEQ, END
    });
  }

  TEST(comments) {
    tokensShouldBe(
      "// single line\n"
      "//< left doc comment\n"
      "//> right doc comment\n"
      "/* multiline\n"
      "   comment */\n"
      "/** multiline\n"
      "    doc comment */\n"
      "/* tricky / // * /* /*/\n"
    , { COMMENT, DOC_COMMENT_L, DOC_COMMENT_R, COMMENT, DOC_COMMENT_R, COMMENT, END }
    );
  }

  TEST(strings) {
    tokensShouldBe(
      "\"a string\"\n"
      "\"string with \\\" excaped quote\"\n"
    , { LIT_STRING, LIT_STRING, END }
    );
  }

  //////////////////////////////////////////////////////////////////////////////

  template<typename ... Args>
  std::string string_format(const std::string& format, Args ... args)
  {
    int size_s = std::snprintf(nullptr, 0, format.c_str(), args ...) + 1;
    if (size_s <= 0) { throw std::runtime_error("Error during formatting."); }
    auto size = static_cast<size_t>(size_s);
    std::unique_ptr<char[]> buf(new char[size]);
    std::snprintf(buf.get(), size, format.c_str(), args ...);
    return std::string(buf.get(), buf.get() + size - 1);
  }

  void tokensShouldBe(const char* text, std::vector<TokenTy> expected) {
    auto observed = Lexer().run(text);
    auto end = observed.size() < expected.size() ? observed.size() : expected.size();
    for (int i = 0; i < end; i++) {
      if (observed[i].ty != expected[i]) {
        throw std::runtime_error(string_format("First mismatched token was at index %d", i));
      }
    }
    if (expected.size() != observed.size())
      throw std::runtime_error(string_format("Expected %d tokens but lexed %d.", expected.size(), observed.size()));
  }
}