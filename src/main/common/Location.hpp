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

  /// @brief True iff the row and col are both nonzero.
  bool exists() const { return row > 0 && col > 0; }

  /// @brief Negation of exists(). True iff `row` or `col` is zero.
  bool notExists() const { return row == 0 || col == 0; }

  /// @brief A location is "true" iff it exists.
  operator bool() const { return exists(); }
};

#endif