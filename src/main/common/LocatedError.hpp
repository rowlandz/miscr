#ifndef COMMON_LOCATEDERROR
#define COMMON_LOCATEDERROR

#include <string>
#include <vector>
#include <llvm/ADT/StringRef.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/Twine.h>
#include "common/Location.hpp"
#include "common/LocationTable.hpp"

#define BOLDBLUE "\x1B[1;34m"
#define RESETCOLOR "\x1B[0m"

/// @brief An error message that can pretty-print source code snippets. The
/// user is provided complete control over the message text as well as a helper
/// method for adding source code snippets. The message is constructed lazily
/// by the efficient stream `<<` operator and `render`ed on demand.
class LocatedError {
public:

  /// @param underlineChar The character used to underline selected text in
  ///        code snippets. `\0` means omit the underline.
  /// @param omitSignifier If true, the initial `error: ` signifier is omitted.
  LocatedError(char underlineChar = '\0', bool omitSignifier = false) {
    this->underlineChar = underlineChar;
    if (!omitSignifier) *this << "\x1B[1;31merror\x1B[37m:\x1B[0m ";
  }

  /// @brief Appends @p text to this error message. The text is not copied into
  /// an owned buffer so the error message must be rendered before @p text is
  /// freed. In practice, this should only be used when @p text is a static
  /// string literal.
  template<unsigned long N>
  LocatedError& operator<<(const char (&text)[N]) {
    fragments.push_back(Fragment(llvm::StringRef(text, N-1), true));
    return *this;
  }

  /// @brief Appends @p text to this error message. The text is copied into an
  /// owned buffer.
  LocatedError& operator<<(llvm::StringRef text) {
    fragments.push_back(Fragment(text, false));
    return *this;
  }

  /// @brief Appends a code snippet to this error message. Code snippets should
  /// only be added directly after newlines, otherwise the text won't line up
  /// correctly. The appended text always ends with a newline. A code snippet
  /// looks something like this:
  /// ```
  /// 42 |   somefunctioncall(arg1, arg2, arg3);
  ///                         ^^^^
  /// ```
  LocatedError& operator<<(Location loc)
    { fragments.push_back(Fragment(loc)); return *this; }

  /// @brief Returns a `std::string` representation of this error message.
  std::string render(const char* srcText, const LocationTable& lt) {
    std::string out;
    for (auto frag = fragments.begin(); frag < fragments.end(); ++frag) {
      switch (frag->tag) {
      case Fragment::TEXT: out.append(frag->text); break;
      case Fragment::STATIC_TEXT: out.append(frag->staticText.str()); break;
      case Fragment::LOCATION: {
        const char* lineBegin = lt.findRow(frag->location.row, srcText);
        renderCodeSnippet(out, frag->location, lineBegin);
      }
      }
    }
    return out;
  }

private:
  struct Fragment {
    enum { TEXT, STATIC_TEXT, LOCATION } tag;
    std::string text;  // putting this inside the union is evil
    union {
      llvm::StringRef staticText;
      Location location;
    };
    Fragment(llvm::StringRef s, bool isStatic) {
      if (isStatic) {
        tag = STATIC_TEXT;
        staticText = s;
      } else {
        tag = TEXT;
        text = s.str();
      }
    }
    Fragment(Location l) : tag(LOCATION), location(l) {}
  };

  char underlineChar;
  llvm::SmallVector<Fragment,4> fragments;

  void renderCodeSnippet(std::string& out, Location loc, const char* lineBegin){
    const char* selectBegin = lineBegin + loc.col - 1;
    const char* selectEnd = selectBegin + loc.sz;

    std::vector<const char*> newlinePositions;
    for (const char* p = selectBegin; p < selectEnd; p++)
      if (*p == '\n') newlinePositions.push_back(p);

    if (newlinePositions.size() == 0) {
      std::string rowMarker = std::to_string(loc.row);
      renderLine(out, rowMarker, lineBegin, selectBegin, selectEnd);
    } else {
      std::vector<std::string> rowMarkers;
      int lastRowToPrint = loc.row + newlinePositions.size();
      for (int i = loc.row; i <= lastRowToPrint; i++)
        rowMarkers.push_back(std::to_string(i));
      int prefixWidth = rowMarkers.back().size();
      int line = 0;

      // first row
      std::string pref = std::string(prefixWidth - rowMarkers[line].size(), ' ')
                       + rowMarkers[line];
      renderLine(out, pref, lineBegin, lineBegin + loc.col - 1, newlinePositions[line]);
      lineBegin = newlinePositions[line] + 1;
      ++line;

      // middle rows
      while (line < newlinePositions.size()) {
        pref = std::string(prefixWidth - rowMarkers[line].size(), ' ')
             + rowMarkers[line];
        renderLine(out, pref, lineBegin, lineBegin, newlinePositions[line]);
        lineBegin = newlinePositions[line] + 1;
        ++line;
      }

      // last row
      pref = std::string(prefixWidth - rowMarkers[line].size(), ' ')
           + rowMarkers[line];
      renderLine(out, pref, lineBegin, lineBegin, selectEnd);
    }
  }

  void renderLine
  ( std::string& out,
    llvm::StringRef prefix,
    const char* lineBegin,
    const char* selectBegin,
    const char* selectEnd
  ) {
    const char* lineEnd = selectEnd;
    while (*lineEnd != '\0' && *lineEnd != '\n') ++lineEnd;
    out.append((BOLDBLUE + prefix + " | " + RESETCOLOR).str());
    out.append(std::string(lineBegin, selectBegin-lineBegin));
    out.append("\x1B[1;35m"); // magenta
    out.append(selectBegin, selectEnd-selectBegin);
    out.append(RESETCOLOR);
    out.append(selectEnd, lineEnd-selectEnd);
    out.append("\n");
    if (underlineChar != '\0') {
      out.append(prefix.size() + 3 + (selectBegin - lineBegin), ' ');
      out.append(selectEnd - selectBegin, underlineChar);
      out.append("\n");
    }
  }

};

#endif