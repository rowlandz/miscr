#ifndef PRINTERS
#define PRINTERS

#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/FormatVariadic.h>
#include "common/AST.hpp"
#include "common/TypeContext.hpp"

void print_parse_tree(
      AST* n,
      std::vector<bool>& indents,
      const TypeContext* tc = nullptr) {
  assert(n != nullptr && "Tried to print nullptr AST node!");
  Location loc = n->getLocation();
  llvm::outs() <<
    llvm::formatv("ln{0,3}, col{1,3}, sz{2,3}   ", loc.row, loc.col, loc.sz);
  if (indents.size() > 0) {
    for (auto i = 0; i < indents.size() - 1; i++) {
      llvm::outs() << (indents[i] ? "│" : " ") << "   ";
    }
    llvm::outs() << (indents.back() ? "├" : "└") << "── ";
  }
  llvm::outs() << ASTIDToString(n->getID());

  // print optional extra information depending on the node type
  if (auto name = Name::downcast(n))
    llvm::outs() << "  (" << name->asStringRef() << ")";
  if (auto intLit = IntLit::downcast(n))
    llvm::outs() << "  (" << intLit->asStringRef() << ")";
  if (auto exp = Exp::downcast(n)) {
    if (tc != nullptr)
      llvm::outs() << " : " << tc->TVarToString(exp->getTVar());
  }

  llvm::outs() << "\n";

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