#include "GraphEngine.h"

void GraphEngine::addNode(std::shared_ptr<GraphNode> node) {
  nodes.push_back(node);
}

void GraphEngine::recalculate() {
  // For Step 2, a hardcoded top-to-bottom iteration suffices since we only have
  // In -> Out. In later phases, this will implement a proper Topological Sort
  // based on connections.
  for (auto &node : nodes) {
    node->process();
  }
}
