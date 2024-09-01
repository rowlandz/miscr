#ifndef PRINTERS
#define PRINTERS

#include "common/Node.hpp"

void print_parse_tree(NodeManager &m, unsigned int n, std::vector<bool> &indents) {
  Node node = m.get(n);
  printf("ln%3d, col%3d, sz%3d   ", node.loc.row, node.loc.col, node.loc.sz);
  if (indents.size() > 0) {
    for (auto i = 0; i < indents.size() - 1; i++) {
      printf("%s   ", indents[i] ? "│" : " ");
    }
    printf("%s── ", *(indents.end()-1) ? "├" : "└");
  }
  printf("%s\n", NodeTyToString(node.ty));

  if (node.n1 != NN) {
    if (node.n2 != NN) {
      if (node.n3 != NN) {
        if (node.extra.nodes.n4 != NN) {              // TODO: this could cause problems
          if (node.extra.nodes.n5 != NN) {
            // five subnodes
            indents.push_back(true);
            print_parse_tree(m, node.n1, indents);
            print_parse_tree(m, node.n2, indents);
            print_parse_tree(m, node.n3, indents);
            print_parse_tree(m, node.extra.nodes.n4, indents);
            indents.pop_back(); indents.push_back(false);
            print_parse_tree(m, node.extra.nodes.n5, indents);
            indents.pop_back();
          } else {
            // four subnodes
            indents.push_back(true);
            print_parse_tree(m, node.n1, indents);
            print_parse_tree(m, node.n2, indents);
            print_parse_tree(m, node.n3, indents);
            indents.pop_back(); indents.push_back(false);
            print_parse_tree(m, node.extra.nodes.n4, indents);
            indents.pop_back();
          }
        } else {
          // three subnodes
          indents.push_back(true);
          print_parse_tree(m, node.n1, indents);
          print_parse_tree(m, node.n2, indents);
          indents.pop_back(); indents.push_back(false);
          print_parse_tree(m, node.n3, indents);
          indents.pop_back();
        }
      } else {
        // two subnodes
        indents.push_back(true);
        print_parse_tree(m, node.n1, indents);
        indents.pop_back(); indents.push_back(false);
        print_parse_tree(m, node.n2, indents);
        indents.pop_back();
      }
    } else {
      // one subnode
      indents.push_back(false);
      print_parse_tree(m, node.n1, indents);
      indents.pop_back();
    }
  }
}

#endif