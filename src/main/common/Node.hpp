#ifndef COMMON_NODE
#define COMMON_NODE

#include "common/Location.hpp"

enum struct NodeTy: unsigned short {

  // Type Nodes

  TYPEVAR, BOOL, i8, i32,

  // Expression Nodes

  EIDENT, LIT_INT, LIT_DEC, LIT_STRING,
  ADD, SUB, MUL, DIV, EQ, NE,
  IF, BLOCK, CALL,

  // Statement Nodes

  LET, RETURN,

  // Declaration Nodes

  FUNC, PROC,

  // Other

  IDENT,
  EXPLIST_NIL, EXPLIST_CONS,
  STMTLIST_NIL, STMTLIST_CONS,
  PARAMLIST_NIL, PARAMLIST_CONS,
};

/**
 * A node in a parse tree. The slots `n1` to `n3` are references to child
 * nodes (or `NN` for empty). An empty slot must not occur before a nonempty
 * slot.
 * 
 * The `n1` slot of all expression nodes is a type.
 */
typedef struct {
  Location loc;
  unsigned int n1;
  unsigned int n2;
  unsigned int n3;
  union {
    const char* ptr;
    long intVal;
    double decVal;
    struct {
      unsigned int n4;
      unsigned int n5;
    } nodes;
  } extra;
  NodeTy ty;
} Node;

#endif
