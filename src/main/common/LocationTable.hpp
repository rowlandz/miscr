#ifndef COMMON_LOCATIONINDEX
#define COMMON_LOCATIONINDEX

#include <map>
#include <cstdio>

/// Maps (some) row numbers to char pointers so that the entire text
/// doesn't need to be scanned to find a location.
class LocationTable {
  std::map<unsigned short, const char*> idx;

public:
  LocationTable(const char* beginningOfText) {
    idx[1] = beginningOfText;
  }
  
  /// Adds an entry to the location table.
  void add(unsigned short row, const char* beginningOfLine) {
    idx[row] = beginningOfLine;
  }

  /// Finds beginning of `row` in `text` using this location table.
  const char* findRow(unsigned short row, const char* text) const {
    std::pair<unsigned short, const char*> closest = *std::prev(idx.upper_bound(row));
    unsigned short r = closest.first;
    const char* p = closest.second;
    while (r < row) {
      while (*p != '\n') p++;
      r++;
      p++;
    }
    return p;
  }
};

#endif