#pragma once

#include "GraphNode.h"
#include <memory>
#include <vector>

class GraphEngine {
public:
  GraphEngine() = default;
  ~GraphEngine() = default;

  void addNode(std::shared_ptr<GraphNode> node);

  // Forces all nodes to recalculate their states downstream
  void recalculate();

private:
  std::vector<std::shared_ptr<GraphNode>> nodes;
};
