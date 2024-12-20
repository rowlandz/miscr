#ifndef PRINTERS
#define PRINTERS

#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/FormatVariadic.h>
#include "common/AST.hpp"
#include "common/TypeContext.hpp"

void print_parse_tree(
      AST* n,
      std::vector<bool>& indents) {
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
  llvm::outs() << n->getIDAsString();

  // print optional extra information depending on the node type
  if (auto binopExp = BinopExp::downcast(n))
    llvm::outs() << " (" << binopExp->getBinopAsEnumString() << ")";
  if (auto unopExp = UnopExp::downcast(n))
    llvm::outs() << " (" << unopExp->getUnopAsEnumString() << ")";
  if (auto funcDecl = FunctionDecl::downcast(n))
    { if (funcDecl->isVariadic()) llvm::outs() << " (variadic)"; }
  if (auto intLit = IntLit::downcast(n))
    llvm::outs() << " (" << intLit->asStringRef() << ")";
  if (auto name = Name::downcast(n))
    llvm::outs() << " (" << name->asStringRef() << ")";
  if (auto primTexp = PrimitiveTypeExp::downcast(n))
    llvm::outs() << " (" << primTexp->getKindAsString() << ")";
  if (auto projectExp = ProjectExp::downcast(n))
    llvm::outs() << " (" << projectExp->getKindAsEnumString() << ")";
  if (auto exp = Exp::downcast(n)) {
    if (Type* ty = exp->getType())
      llvm::outs() << " : " << ty->asString();
  }

  llvm::outs() << "\n";

  auto subnodes = n->getASTChildren();
  if (subnodes.size() == 1) {
    indents.push_back(false);
    print_parse_tree(subnodes[0], indents);
    indents.pop_back();
  } else if (subnodes.size() >= 2) {
    indents.push_back(true);
    for (auto subnode = subnodes.begin(); subnode < subnodes.end()-1; subnode++) {
      print_parse_tree(*subnode, indents);
    }
    indents.pop_back();
    indents.push_back(false);
    print_parse_tree(subnodes.back(), indents);
    indents.pop_back();
  }
}

#endif