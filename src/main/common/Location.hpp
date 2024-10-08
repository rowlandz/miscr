#ifndef COMMON_LOCATION
#define COMMON_LOCATION

/// A range of text.
class Location {
public:
  unsigned short row;
  unsigned short col;
  unsigned int sz;

  /// @brief Constructs the default location (0,0,0).
  Location() : row(0), col(0), sz(0) {}

  Location(unsigned short row, unsigned short col, unsigned int sz)
    : row(row), col(col), sz(sz) {}
};

#endif