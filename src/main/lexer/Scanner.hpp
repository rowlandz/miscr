#ifndef LEXER_SCANNER
#define LEXER_SCANNER

#include <vector>
#include <llvm/ADT/StringRef.h>
#include "common/Token.hpp"
#include "common/Location.hpp"
#include "common/LocationTable.hpp"

/// @brief Defines low-level functionality associated with scanning text and
/// generating a token stream.
///
/// Separating this from the Lexer helps keep the Lexer itself concise and free
/// of clutter. A Scanner has three jobs:
///   - Efficiently calculates a source code Location for each Token.
///   - Builds a LocationTable.
///   - Maintains the lexer state of type S.
///
/// Below is some sample text that is being scanned.
///
/// @code
///    voila sample text
///          ~~~^
/// @endcode
///
/// The _selection_ is "sam". The _cursor_ points to the _current char_
/// which is 'p'. step() would make the selection "samp" and move the cursor to
/// 'l'. capture() would empty the selection and push "sam" onto the token
/// stream. discard() would empty the selection without pushing a token.
template<typename S>
class Scanner {
public:
  Scanner(llvm::StringRef text, S _initialState,
          LocationTable* locationTable = nullptr) : _locTable(locationTable) {
    p1 = p2 = text.data();
    row = 1; col = 1;
    newlines = 0;
    lastNewline = nullptr;
    _state = initialState = _initialState;
  }

  /// @brief Moves the cursor one char to the right (which adds the current
  /// char to the selection) and sets the state to `newState`.
  void step(S newState) {
    if (*p2 == '\n') {
      newlines++;
      lastNewline = p2;
      if (((row + newlines) & 0x03) == 0) {
        if (_locTable != nullptr)
          _locTable->add(row + newlines, p2 + 1);
      }
    }
    p2++;
    _state = newState;
  }

  /// @brief Captures the selected characters as a new token with @p tag.
  /// The selection is cleared and state is reset to the initial state.
  void capture(Token::Tag tag) {
    u_int8_t tokenSize = (u_int8_t)(p2 - p1);
    _tokens.push_back(Token(tag, p1, Location(row, col, tokenSize)));
    _state = initialState;
    p1 = p2;
    if (newlines == 0) {
      col += tokenSize;
    } else {
      row += newlines;
      col = (u_int8_t)(p2 - lastNewline);
    }
    newlines = 0;
    lastNewline = nullptr;
  }

  /// @brief Starts the selection over at current cursor position and resets
  /// state to the initial state.
  void discard() {
    u_int8_t tokenSize = (u_int8_t)(p2 - p1);
    _state = initialState;
    p1 = p2;
    if (newlines == 0) {
      col += tokenSize;
    } else {
      row += newlines;
      col = (u_int8_t)(p2 - lastNewline);
    }
    newlines = 0;
    lastNewline = nullptr;
  }

  /// @brief Equivalent to step() followed by capture(tag)
  void stepAndCapture(Token::Tag tag) {
    step(initialState);
    capture(tag);
  }

  /// @brief Equivalent to step() followed by discard().
  void stepAndDiscard() {
    step(initialState);
    discard();
  }

  /// @brief Returns the current token state.
  S state() { return _state; }

  /// @brief Returns the token stream.
  const std::vector<Token>& tokens() { return _tokens; }

  /// @brief Returns true iff there are unprocessed chars left. The end of the
  /// character buffer is indicated by a null character.
  bool thereAreMoreChars() { return *p2 != '\0'; }

  /// @brief Returns the first unprocessed char (the current char).
  char currentChar() { return *p2; }

  /// @brief Returns the current selection. 
  llvm::StringRef selection() { return llvm::StringRef(p1, p2 - p1); }

  /// @brief Returns the Location of the current selection.
  Location selectionLocation()
    { return Location(row, col, static_cast<unsigned int>(p2 - p1)); }

  /// @brief Returns the location (with size 1) of the first character of the
  /// current selection.
  Location selectionBeginLocation() { return Location(row, col, 1); }

private:
  const char* p1;                // beginning of selection
  const char* p2;                // cursor position
  unsigned short row;            // row of beginning of selection
  unsigned short col;            // col of beginning of selection
  u_int8_t newlines;             // number of newlines in the current selection
  const char* lastNewline;       // points to last newline in selection (or nullptr if none exist).
  S _state;                      // current state
  S initialState;                // initial state
  std::vector<Token> _tokens;    // token accumulator
  LocationTable* _locTable;      // location table that is built up during lexing.
};

#endif