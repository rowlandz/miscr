#ifndef PRINTERS
#define PRINTERS

#include "common/ASTContext.hpp"

void print_parse_tree(ASTContext& ctx, Addr<AST> n, std::vector<bool>& indents) {
  if (n.isError()) {
    printf("Tried to print an error node!\n");
    exit(1);
  }
  AST node = ctx.get(n);
  if (node.isLocated()) {
    Location loc = ctx.get(n.UNSAFE_CAST<LocatedAST>()).getLocation();
    printf("ln%3d, col%3d, sz%3d   ", loc.row, loc.col, loc.sz);
  }
  if (indents.size() > 0) {
    for (auto i = 0; i < indents.size() - 1; i++) {
      printf("%s   ", indents[i] ? "│" : " ");
    }
    printf("%s── ", *(indents.end()-1) ? "├" : "└");
  }
  printf("%s ", ASTIDToString(node.getID()));

  // print optional extra information depending on the node type
  if (node.getID() == AST::ID::IDENT)
    printf("(%s)", ctx.get(n.UNSAFE_CAST<Ident>()).asString().c_str());
  else if (node.getID() == AST::ID::INT_LIT)
    printf("(%s)", ctx.get(n.UNSAFE_CAST<IntLit>()).asString().c_str());
  printf("\n");

  auto subnodes = getSubnodes(ctx, n);
  if (subnodes.size() == 1) {
    indents.push_back(false);
    print_parse_tree(ctx, subnodes[0], indents);
    indents.pop_back();
  } else if (subnodes.size() >= 2) {
    indents.push_back(true);
    for (auto subnode = subnodes.begin(); subnode < subnodes.end()-1; subnode++) {
      print_parse_tree(ctx, *subnode, indents);
    }
    indents.pop_back();
    indents.push_back(false);
    print_parse_tree(ctx, subnodes.back(), indents);
    indents.pop_back();
  }
}

#endif