#ifndef COMMON_LOCATION
#define COMMON_LOCATION

/// A range of text.
class Location {
public:
  unsigned short row;
  unsigned short col;
  unsigned int sz;

  /// @brief Default location is 0,0,0
  Location() { row = 0; col = 0; sz = 0; }

  Location(unsigned short row, unsigned short col, unsigned int sz) {
    this->row = row; this->col = col; this->sz = sz;
  }
};

#endif