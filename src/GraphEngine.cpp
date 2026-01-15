#include "GraphEngine.h"
#include <algorithm>

void GraphEngine::addNode(std::shared_ptr<GraphNode> node) {
  nodes.push_back(node);
  updateImplicitConnections();
}

void GraphEngine::removeNode(GraphNode *node) {
  auto it = std::remove_if(
      nodes.begin(), nodes.end(),
      [node](const std::shared_ptr<GraphNode> &n) { return n.get() == node; });
  if (it != nodes.end()) {
    nodes.erase(it, nodes.end());
    updateImplicitConnections();
  }
}

void GraphEngine::moveNode(GraphNode *node, int newIndex) {
  auto it = std::find_if(
      nodes.begin(), nodes.end(),
      [node](const std::shared_ptr<GraphNode> &n) { return n.get() == node; });

  if (it != nodes.end()) {
    std::shared_ptr<GraphNode> n = *it;
    nodes.erase(it);

    if (newIndex < 0)
      newIndex = 0;
    if (newIndex > (int)nodes.size())
      newIndex = (int)nodes.size();

    nodes.insert(nodes.begin() + newIndex, n);
    updateImplicitConnections();
  }
}

void GraphEngine::updateImplicitConnections() {
  // Clear all connections first
  for (auto &node : nodes) {
    node->clearConnections();
  }

  // Create implicit connections from node N output 0 to node N+1 input 0
  for (size_t i = 0; i + 1 < nodes.size(); ++i) {
    nodes[i]->addConnection(0, nodes[i + 1].get(), 0);
  }
}

void GraphEngine::recalculate() {
  // Linear processing for Step 7 (Implicit flow)
  // Each node processes using its input sequences which it gathers from
  // upstream

  for (size_t i = 0; i < nodes.size(); ++i) {
    // Before processing, we must copy the output of the previous node to our
    // input because we are using a linear Top-to-Bottom evaluation model.
    if (i > 0) {
      // Find all connections that point to us
      for (size_t j = 0; j < i; ++j) {
        for (const auto &connList : nodes[j]->getConnections()) {
          int outPort = connList.first;
          for (const auto &conn : connList.second) {
            if (conn.targetNode == nodes[i].get()) {
              nodes[i]->setInputSequence(conn.targetInputPort,
                                         nodes[j]->getOutputSequence(outPort));
            }
          }
        }
      }
    }
    nodes[i]->process();
  }
}
