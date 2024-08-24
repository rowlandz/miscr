#ifndef COMMON_NODEMANAGER
#define COMMON_NODEMANAGER

#include <vector>
#include "common/Node.hpp"

class NodeManager {

public:
  std::vector<Node> nodes;

  Node get(unsigned int addr) const {
    return nodes[addr];
  }

  void set(unsigned int addr, Node node) {
    nodes[addr] = node;
  }

  unsigned int add(Node node) {
    nodes.push_back(node);
    return (unsigned int)(nodes.size() - 1);
  }
};

#endif