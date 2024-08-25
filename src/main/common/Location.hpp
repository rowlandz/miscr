#ifndef COMMON_LOCATION
#define COMMON_LOCATION

/** A range of text. */
typedef struct Location {
  unsigned short row;
  unsigned short col;
  unsigned int sz;
} Location;

#endif