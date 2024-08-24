#ifndef LEXER_SCANNER
#define LEXER_SCANNER

#include <cstdlib>            // TODO: can we remove this?
#include <vector>
#include "common/Token.hpp"

/** Abstracts out the functionality of scanning text and identifying tokens.
 * 
 ```
    voila sample text
          ~~~^
 ```
 * selected text is "sam"
 * current char is "p"
 * `step` would make the selection "samp" and current char "l"
 * `capture` would empty the selection and make "sam" a new token
 * `discard` would empty the selection without making a new token
 **/
template<typename S>
class Scanner {

public:
  Scanner() {}
  Scanner(const char* text, S _initialState) {
    p1 = p2 = text;
    row = 1; col = 1;
    newlines = 0;
    lastNewline = nullptr;
    _state = initialState = _initialState;
  }

  /** Moves the cursor one char to the right (which adds the current char
   * to the selection) and sets the state to `newState`.
   * @note the current character should *not* be a newline. */
  void step(S newState) {
    if (*p2 == '\n') { newlines++; lastNewline = p2; }
    p2++;
    _state = newState;
  }

  /** Captures selected characters as a new token of type `ty`.
   * Selection is cleared and state is reset. */
  void capture(TokenTy ty) {
    u_int8_t tokenSize = (u_int8_t)(p2 - p1);
    _tokens.push_back({ p1, row, col, tokenSize, ty });
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

  /** Starts selection over at current cursor position and resets state. */
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

  /** Equivalent to `step(_); capture(ty)` */
  void stepAndCapture(TokenTy ty) {
    step(initialState);
    capture(ty);
  }

  /** Equivalent to `step(_); discard()` */
  void stepAndDiscard() {
    step(initialState);
    discard();
  }

  /** Returns the current token state. */
  S state() { return _state; }

  /** Returns the tokens. */
  std::vector<Token> tokens() { return _tokens; }

  /** Returns true if there are unprocessed chars left. */
  bool thereAreMoreChars() { return *p2 != '\0'; }

  /** Returns the first unprocessed char (the current char). */
  char currentChar() { return *p2; }

  /** Returns a pointer to the selection. */
  const char* selectionPtr() { return p1; }

  /** Returns the number of selected chars. */
  u_int16_t selectionSize() { return p2 - p1; }

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
};

#endif