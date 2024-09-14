#ifndef PRINTERS
#define PRINTERS

#include "common/Ontology.hpp"
#include "common/ASTContext.hpp"
#include "common/TypeContext.hpp"

void print_parse_tree(
      const ASTContext& ctx,
      Addr<AST> n,
      std::vector<bool>& indents,
      const TypeContext* tc = nullptr,
      const Ontology* ont = nullptr) {
  assert(n.exists() && "Tried to print error AST node!");
  AST node = ctx.get(n);
  Location loc = ctx.get(n.UNSAFE_CAST<LocatedAST>()).getLocation();
  printf("ln%3d, col%3d, sz%3d   ", loc.row, loc.col, loc.sz);
  if (indents.size() > 0) {
    for (auto i = 0; i < indents.size() - 1; i++) {
      printf("%s   ", indents[i] ? "│" : " ");
    }
    printf("%s── ", *(indents.end()-1) ? "├" : "└");
  }
  printf("%s", ASTIDToString(node.getID()));

  // print optional extra information depending on the node type
  if (node.getID() == AST::ID::IDENT)
    printf(" (%s)", ctx.GET_UNSAFE<Ident>(n).asString().c_str());
  if (node.getID() == AST::ID::FQIDENT)
    printf(" (%s)", ont->getName(ctx.GET_UNSAFE<FQIdent>(n).getKey()).c_str());
  if (node.getID() == AST::ID::INT_LIT)
    printf(" (%s)", ctx.GET_UNSAFE<IntLit>(n).asString().c_str());
  if (node.isExp() && tc != nullptr)
    printf(" : %s", tc->TVarToString(ctx.GET_UNSAFE<Exp>(n).getTVar()).c_str());

  printf("\n");

  auto subnodes = getSubnodes(ctx, n);
  if (subnodes.size() == 1) {
    indents.push_back(false);
    print_parse_tree(ctx, subnodes[0], indents, tc, ont);
    indents.pop_back();
  } else if (subnodes.size() >= 2) {
    indents.push_back(true);
    for (auto subnode = subnodes.begin(); subnode < subnodes.end()-1; subnode++) {
      print_parse_tree(ctx, *subnode, indents, tc, ont);
    }
    indents.pop_back();
    indents.push_back(false);
    print_parse_tree(ctx, subnodes.back(), indents, tc, ont);
    indents.pop_back();
  }
}

#endif