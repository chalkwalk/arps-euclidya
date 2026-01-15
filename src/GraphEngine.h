// GraphEngine.h
#pragma once

#include "GraphNode.h"
#include <memory>
#include <vector>

class GraphEngine {
public:
  GraphEngine() = default;
  ~GraphEngine() = default;

  void addNode(std::shared_ptr<GraphNode> node);
  void removeNode(GraphNode *node);
  void moveNode(GraphNode *node, int newIndex);

  // Forces all nodes to recalculate their states downstream
  void recalculate();

  // Gets the current list of nodes (in order)
  const std::vector<std::shared_ptr<GraphNode>> &getNodes() const {
    return nodes;
  }

  // Rebuilds implicit connections based on node order and applies overrides
  void updateImplicitConnections();

private:
  std::vector<std::shared_ptr<GraphNode>> nodes;
};
