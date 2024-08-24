#ifndef COMMON_LOCATION
#define COMMON_LOCATION

/** A range of text. */
typedef struct Location {
  const char* ptr;
  unsigned int sz;
  unsigned short row;
  unsigned short col;
} Location;

#endif