#ifndef COMMON_NODE
#define COMMON_NODE

#include <string>
#include "common/Location.hpp"

/**
 * The type of a node determines how the node's other fields should be used.
 * This is _not_ the same as the types of expressions.
 */
enum struct NodeTy: unsigned short {

  // expression nodes

  EQIDENT, EFQIDENT, LIT_INT, LIT_DEC, LIT_STRING, TRUE, FALSE,
  ADD, SUB, MUL, DIV, EQ, NE,
  ARRAY_CONSTR_LIST, ARRAY_CONSTR_INIT,
  IF, BLOCK, CALL, ASCRIP,

  // statement nodes

  LET, RETURN,

  // declaration nodes

  EXTERN_FUNC, EXTERN_PROC, FUNC, PROC, MODULE, NAMESPACE,

  // type nodes

  TYPEVAR, UNIT, BOOL, i8, i32, f32, f64, STRING,
  TY_ARRAY, TY_REF,
  NUMERIC, DECIMAL,

  // Other

  IDENT, QIDENT, FQIDENT,
  EXPLIST_NIL, EXPLIST_CONS,
  STMTLIST_NIL, STMTLIST_CONS,
  PARAMLIST_NIL, PARAMLIST_CONS,
  DECLLIST_NIL, DECLLIST_CONS,
};

bool isExpNodeTy(NodeTy ty) {
  switch (ty) {
    case NodeTy::EQIDENT:
    case NodeTy::EFQIDENT:
    case NodeTy::LIT_INT:
    case NodeTy::LIT_DEC:
    case NodeTy::LIT_STRING:
    case NodeTy::TRUE:
    case NodeTy::FALSE:
    case NodeTy::ADD:
    case NodeTy::SUB:
    case NodeTy::MUL:
    case NodeTy::DIV:
    case NodeTy::EQ:
    case NodeTy::NE:
    case NodeTy::ARRAY_CONSTR_LIST:
    case NodeTy::ARRAY_CONSTR_INIT:
    case NodeTy::IF:
    case NodeTy::BLOCK:
    case NodeTy::CALL:
    case NodeTy::ASCRIP:
      return true;
    default: return false;
  }
}

const char* NodeTyToString(NodeTy nt) {
  switch (nt) {
    case NodeTy::LIT_INT:          return "LIT_INT";
    case NodeTy::LIT_DEC:          return "LIT_DEC";
    case NodeTy::LIT_STRING:       return "LIT_STRING";
    case NodeTy::FALSE:            return "FALSE";
    case NodeTy::TRUE:             return "TRUE";
    case NodeTy::IF:               return "IF";
    case NodeTy::ADD:              return "ADD";
    case NodeTy::SUB:              return "SUB";
    case NodeTy::MUL:              return "MUL";
    case NodeTy::DIV:              return "DIV";
    case NodeTy::EQ:               return "EQ";
    case NodeTy::NE:               return "NE";
    case NodeTy::ARRAY_CONSTR_LIST:   return "ARRAY_CONSTR_LIST";
    case NodeTy::ARRAY_CONSTR_INIT:   return "ARRAY_CONSTR_INIT";
    case NodeTy::BLOCK:            return "BLOCK";
    case NodeTy::CALL:             return "CALL";
    case NodeTy::ASCRIP:           return "ASCRIP";
    case NodeTy::UNIT:             return "UNIT";
    case NodeTy::BOOL:             return "BOOL";
    case NodeTy::TYPEVAR:          return "TYPEVAR";
    case NodeTy::NUMERIC:          return "NUMERIC";
    case NodeTy::DECIMAL:          return "DECIMAL";
    case NodeTy::f32:              return "f32";
    case NodeTy::f64:              return "f64";
    case NodeTy::i8:               return "i8";
    case NodeTy::i32:              return "i32";
    case NodeTy::STRING:           return "STRING";
    case NodeTy::TY_ARRAY:         return "TY_ARRAY";
    case NodeTy::TY_REF:           return "TY_REF";
    case NodeTy::IDENT:            return "IDENT";
    case NodeTy::QIDENT:           return "QIDENT";
    case NodeTy::FQIDENT:          return "FQIDENT";
    case NodeTy::EQIDENT:          return "EQIDENT";
    case NodeTy::EFQIDENT:         return "EFQIDENT";
    case NodeTy::EXPLIST_NIL:      return "EXPLIST_NIL";
    case NodeTy::EXPLIST_CONS:     return "EXPLIST_CONS";
    case NodeTy::LET:              return "LET";
    case NodeTy::RETURN:           return "RETURN";
    case NodeTy::EXTERN_FUNC:      return "EXTERN_FUNC";
    case NodeTy::EXTERN_PROC:      return "EXTERN_PROC";
    case NodeTy::FUNC:             return "FUNC";
    case NodeTy::PROC:             return "PROC";
    case NodeTy::MODULE:           return "MODULE";
    case NodeTy::NAMESPACE:        return "NAMESPACE";
    case NodeTy::PARAMLIST_NIL:    return "PARAMLIST_NIL";
    case NodeTy::PARAMLIST_CONS:   return "PARAMLIST_CONS";
    case NodeTy::STMTLIST_NIL:     return "STMTLIST_NIL";
    case NodeTy::STMTLIST_CONS:    return "STMTLIST_CONS";
    case NodeTy::DECLLIST_NIL:     return "DECLLIST_NIL";
    case NodeTy::DECLLIST_CONS:    return "DECLLIST_CONS";
  }
}

NodeTy stringToNodeTy(const std::string& str) {
  if      (str == "LIT_INT")          return NodeTy::LIT_INT;
  else if (str == "LIT_DEC")          return NodeTy::LIT_DEC;
  else if (str == "LIT_STRING")       return NodeTy::LIT_STRING;
  else if (str == "FALSE")            return NodeTy::FALSE;
  else if (str == "TRUE")             return NodeTy::TRUE;
  else if (str == "IF")               return NodeTy::IF;
  else if (str == "ADD")              return NodeTy::ADD;
  else if (str == "SUB")              return NodeTy::SUB;
  else if (str == "MUL")              return NodeTy::MUL;
  else if (str == "DIV")              return NodeTy::DIV;
  else if (str == "EQ")               return NodeTy::EQ;
  else if (str == "NE")               return NodeTy::NE;
  else if (str == "ARRAY_CONSTR_LIST")   return NodeTy::ARRAY_CONSTR_LIST;
  else if (str == "ARRAY_CONSTR_INIT")   return NodeTy::ARRAY_CONSTR_INIT;
  else if (str == "BLOCK")            return NodeTy::BLOCK;
  else if (str == "CALL")             return NodeTy::CALL;
  else if (str == "ASCRIP")           return NodeTy::ASCRIP;
  else if (str == "BOOL")             return NodeTy::BOOL;
  else if (str == "TYPEVAR")          return NodeTy::TYPEVAR;
  else if (str == "UNIT")             return NodeTy::UNIT;
  else if (str == "NUMERIC")          return NodeTy::NUMERIC;
  else if (str == "DECIMAL")          return NodeTy::DECIMAL;
  else if (str == "f32")              return NodeTy::f32;
  else if (str == "f64")              return NodeTy::f64;
  else if (str == "i8")               return NodeTy::i8;
  else if (str == "i32")              return NodeTy::i32;
  else if (str == "STRING")           return NodeTy::STRING;
  else if (str == "IDENT")            return NodeTy::IDENT;
  else if (str == "QIDENT")           return NodeTy::QIDENT;
  else if (str == "FQIDENT")          return NodeTy::FQIDENT;
  else if (str == "EQIDENT")          return NodeTy::EQIDENT;
  else if (str == "EFQIDENT")         return NodeTy::EFQIDENT;
  else if (str == "EXPLIST_NIL")      return NodeTy::EXPLIST_NIL;
  else if (str == "EXPLIST_CONS")     return NodeTy::EXPLIST_CONS;
  else if (str == "LET")              return NodeTy::LET;
  else if (str == "RETURN")           return NodeTy::RETURN;
  else if (str == "EXTERN_FUNC")      return NodeTy::EXTERN_FUNC;
  else if (str == "EXTERN_PROC")      return NodeTy::EXTERN_PROC;
  else if (str == "FUNC")             return NodeTy::FUNC;
  else if (str == "PROC")             return NodeTy::PROC;
  else if (str == "MODULE")           return NodeTy::MODULE;
  else if (str == "NAMESPACE")        return NodeTy::NAMESPACE;
  else if (str == "PARAMLIST_NIL")    return NodeTy::PARAMLIST_NIL;
  else if (str == "PARAMLIST_CONS")   return NodeTy::PARAMLIST_CONS;
  else if (str == "STMTLIST_NIL")     return NodeTy::STMTLIST_NIL;
  else if (str == "STMTLIST_CONS")    return NodeTy::STMTLIST_CONS;
  else if (str == "DECLLIST_NIL")     return NodeTy::DECLLIST_NIL;
  else if (str == "DECLLIST_CONS")    return NodeTy::DECLLIST_CONS;
  else { printf("Invalid NodeTy string: %s\n", str.c_str()); exit(1); }
}


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

/** Placed in sub-node slots in a node to indicate "no node" */
#define NN              0xffffffff

/** Place in the extra slot when it's not used. */
#define NOEXTRA         { .nodes={ NN, NN } }

#endif
