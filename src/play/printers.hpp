#ifndef PRINTERS
#define PRINTERS

#include "common/AST.hpp"
#include "common/TypeContext.hpp"

void print_parse_tree(
      AST* n,
      std::vector<bool>& indents,
      const TypeContext* tc = nullptr) {
  assert(n != nullptr && "Tried to print nullptr AST node!");
  Location loc = n->getLocation();
  printf("ln%3d, col%3d, sz%3d   ", loc.row, loc.col, loc.sz);
  if (indents.size() > 0) {
    for (auto i = 0; i < indents.size() - 1; i++) {
      printf("%s   ", indents[i] ? "│" : " ");
    }
    printf("%s── ", *(indents.end()-1) ? "├" : "└");
  }
  printf("%s", ASTIDToString(n->getID()));

  // print optional extra information depending on the node type
  if (auto name = Name::downcast(n))
    printf("  (%s)", name->asStringRef().data());
  if (auto intLit = IntLit::downcast(n))
    printf(" (%s)", intLit->asString().c_str());     // TODO: change to asStringRef
  if (auto exp = Exp::downcast(n)) {
    if (tc != nullptr)
      printf(" : %s", tc->TVarToString(exp->getTVar()).c_str());
  }

  printf("\n");

  auto subnodes = getSubnodes(n);
  if (subnodes.size() == 1) {
    indents.push_back(false);
    print_parse_tree(subnodes[0], indents, tc);
    indents.pop_back();
  } else if (subnodes.size() >= 2) {
    indents.push_back(true);
    for (auto subnode = subnodes.begin(); subnode < subnodes.end()-1; subnode++) {
      print_parse_tree(*subnode, indents, tc);
    }
    indents.pop_back();
    indents.push_back(false);
    print_parse_tree(subnodes.back(), indents, tc);
    indents.pop_back();
  }
}

#endif