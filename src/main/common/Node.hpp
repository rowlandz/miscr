#ifndef COMMON_NODE
#define COMMON_NODE

#include "common/Location.hpp"

enum struct NodeTy: unsigned short {
  LIT_INT, LIT_DEC, LIT_STRING,
  ADD, SUB, MUL, DIV,
  EQ, NE,
  IF,
  BLOCK,
  CALL,
  BOOL, i8, i32,
  IDENT,
  EXPLIST_NIL, EXPLIST_CONS,
  LET, RETURN,
  STMTLIST_NIL, STMTLIST_CONS,
  FUNC, PROC,
  PARAMLIST_NIL, PARAMLIST_CONS,
  SIGNATURE,
};

typedef union {
  const char* ptr;
  long numericValue;
  struct {
    unsigned int n4;
    unsigned int n5;
  } nodes;
} ExtraField;

/**
 * A node in a parse tree. The slots `n1` to `n3` are references to child
 * nodes (or `NN` for empty). An empty slot must not occur before a nonempty
 * slot.
 */
typedef struct {
  Location loc;
  unsigned int n1;
  unsigned int n2;
  unsigned int n3;
  NodeTy ty;
} Node;

#endif

/*

func getOrElse<A>(opt: Option<A>, alt: A): A = opt match {
  Some(a) => a
  None => alt
}

*/

/* location        8 bytes
 *   row               2 bytes
 *   col               2 bytes
 *   sz                4 bytes
 * n1              4 bytes
 * n2              4 bytes
 * n3              4 bytes
 * extra           8 bytes
 * ty              2 bytes


 */