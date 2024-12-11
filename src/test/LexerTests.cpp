#include <llvm/Support/FormatVariadic.h>
#include "lexer/Lexer.hpp"
#include "test.hpp"

namespace LexerTests {
  TESTGROUP("Lexer Tests")

  /// @brief Succeeds if @p text is lexed into @p expected.
  std::optional<std::string>
  tokensShouldBe(const char* text, std::vector<Token::Tag> expected) {
    std::vector<Token> observed = Lexer(text).run();
    if (observed.size() != expected.size()) {
      return llvm::formatv("Got {0} tokens but expected {1}",
        observed.size(), expected.size());
    }
    std::size_t end = observed.size();
    for (int i = 0; i < end; i++) {
      if (observed[i].tag != expected[i]) {
        return llvm::formatv("First mismatched token is at index {0}", i);
      }
    }
    SUCCESS
  }

  //==========================================================================//

  TEST(simple_example_1) {
    return tokensShouldBe("(1 + 2) * 3", {
      Token::LPAREN, Token::LIT_INT, Token::OP_ADD, Token::LIT_INT,
      Token::RPAREN, Token::OP_MUL, Token::LIT_INT, Token::END
    });
  }

  TEST(keywords_and_identifiers) {
    return tokensShouldBe("func funcy let", {
      Token::KW_FUNC, Token::IDENT, Token::KW_LET, Token::END
    });
  }

  TEST(operators) {
    return tokensShouldBe("=  =>  ==  /=", {
      Token::EQUAL, Token::FATARROW, Token::OP_EQ, Token::OP_NE, Token::END
    });
  }

  TEST(ampersands_should_all_be_separate) {
    return tokensShouldBe("&   &&   &&&", {
      Token::AMP, Token::AMP, Token::AMP, Token::AMP, Token::AMP, Token::AMP,
      Token::END
    });
  }

  TEST(comments) {
    return tokensShouldBe(
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
    return tokensShouldBe(
      "\"a string\"\n"
      "\"string with \\\" escaped quote\"\n"
    , { Token::LIT_STRING, Token::LIT_STRING, Token::END }
    );
  }
}