#ifndef COMMON_LOCATEDERROR
#define COMMON_LOCATEDERROR

#include <string>
#include <vector>
#include "common/Location.hpp"
#include "common/LocationTable.hpp"

#define BOLDRED "\x1B[1;31m"
#define BOLDBLUE "\x1B[1;34m"
#define BOLDWHITE "\x1B[1;37m"
#define RESETCOLOR "\x1B[0m"

/** A string of error text paired with a Location. */
class LocatedError {
public:
  Location location;
  std::string errorMsg;

  LocatedError() {}

  LocatedError(Location location, std::string errorMsg) {
    this->location = location;
    this->errorMsg = errorMsg;
  }

  std::string render(const char* text, const LocationTable* lt) {
    const char* beginningOfLine = lt->findRow(location.row, text);
    const char* beginningOfSelection = beginningOfLine + location.col - 1;
    const char* endOfSelection = beginningOfSelection + location.sz;

    std::vector<const char*> newlinePositions;
    for (const char* p = beginningOfSelection; p < endOfSelection; p++) {
      if (*p == '\n') newlinePositions.push_back(p);
    }

    std::string ret = BOLDRED + ("error: " + (BOLDWHITE + errorMsg)) + "\n";
    if (newlinePositions.size() == 0) {
      const char* endOfLine;
      for (endOfLine = beginningOfSelection; *endOfLine != '\0' && *endOfLine != '\n'; endOfLine++);
      std::string rowMarker = std::to_string(location.row);
      ret.append(BOLDBLUE + rowMarker + " | " + RESETCOLOR);
      ret.append(std::string(beginningOfLine, endOfLine - beginningOfLine));
      ret.append("\n");
      ret.append(rowMarker.size() + 3 + location.col - 1, ' ');
      ret.append(location.sz, '^');
      ret.append("\n");
      return ret;
    } else {
      int lastRowToPrint = location.row + newlinePositions.size();
      std::vector<std::string> rowMarkers;
      for (int i = location.row; i <= lastRowToPrint; i++) rowMarkers.push_back(std::to_string(i));
      int rowMarkerWidth = rowMarkers.back().size();

      int line = 0;

      // first row
      ret.append(rowMarkerWidth - rowMarkers[line].size(), ' ');
      ret.append(rowMarkers[line]);
      ret.append(" | ");
      ret.append(std::string(beginningOfLine, newlinePositions[line] - beginningOfLine));
      ret.append("\n");
      ret.append(rowMarkerWidth + 3 + location.col - 1, ' ');
      ret.append(newlinePositions[line] - beginningOfLine - location.col + 1, '^');
      ret.append("\n");
      beginningOfLine = newlinePositions[line] + 1;
      line++;

      // middle rows
      while (line < newlinePositions.size()) {
        ret.append(rowMarkerWidth - rowMarkers[line].size(), ' ');
        ret.append(rowMarkers[line]);
        ret.append(" | ");
        ret.append(std::string(beginningOfLine, newlinePositions[line] - beginningOfLine));
        ret.append("\n");
        ret.append(rowMarkerWidth + 3, ' ');
        ret.append(newlinePositions[line] - beginningOfLine, '^');
        ret.append("\n");
        beginningOfLine = newlinePositions[line] + 1;
        line++;
      }

      // last row
      const char* endOfLastRow;
      for (endOfLastRow = beginningOfLine; *endOfLastRow != '\0' && *endOfLastRow != '\n'; endOfLastRow++);
      ret.append(rowMarkers[line]);
      ret.append(" | ");
      ret.append(std::string(beginningOfLine, endOfLastRow - beginningOfLine));
      ret.append("\n");
      ret.append(rowMarkerWidth + 3, ' ');
      ret.append(endOfSelection - beginningOfLine, '^');
      ret.append("\n");

      return ret;
    }
  }
};

#endif