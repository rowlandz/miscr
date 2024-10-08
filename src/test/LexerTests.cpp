#include <memory>
#include <cstring>
#include "lexer/Lexer.hpp"
#include "test.hpp"

namespace LexerTests {
  TESTGROUP("Lexer Tests")

  /** Succeeds if `text` is lexed into `expected`. */
  void tokensShouldBe(const char* text, std::vector<Token::Tag> expected);

  TEST(simple_example_1) {
    tokensShouldBe("(1 + 2) * 3", {
      Token::LPAREN, Token::LIT_INT, Token::OP_ADD, Token::LIT_INT,
      Token::RPAREN, Token::OP_MUL, Token::LIT_INT, Token::END
    });
  }

  TEST(keywords_and_identifiers) {
    tokensShouldBe("func funcy let", {
      Token::KW_FUNC, Token::TOK_IDENT, Token::KW_LET, Token::END
    });
  }

  TEST(operators) {
    tokensShouldBe("=  =>  ==  /=", {
      Token::EQUAL, Token::FATARROW, Token::OP_EQ, Token::OP_NE, Token::END
    });
  }

  TEST(ampersands_should_all_be_separate) {
    tokensShouldBe("&   &&   &&&", {
      Token::AMP, Token::AMP, Token::AMP, Token::AMP, Token::AMP, Token::AMP,
      Token::END
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
    , { Token::DOC_COMMENT_L, Token::DOC_COMMENT_R, Token::DOC_COMMENT_R, Token::END }
    );
  }

  TEST(strings) {
    tokensShouldBe(
      "\"a string\"\n"
      "\"string with \\\" excaped quote\"\n"
    , { Token::LIT_STRING, Token::LIT_STRING, Token::END }
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

  void tokensShouldBe(const char* text, std::vector<Token::Tag> expected) {
    Lexer lexer(text);
    lexer.run();
    auto observed = lexer.getTokens();
    auto end = observed.size() < expected.size() ? observed.size() : expected.size();
    for (int i = 0; i < end; i++) {
      if (observed.at(i).tag != expected[i]) {
        throw std::runtime_error(string_format("First mismatched token was at index %d", i));
      }
    }
    if (expected.size() != observed.size())
      throw std::runtime_error(string_format("Expected %d tokens but lexed %d.", expected.size(), observed.size()));
  }
}