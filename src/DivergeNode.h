#pragma once
#include "GraphNode.h"

class DivergeNode : public GraphNode {
public:
  DivergeNode() = default;
  std::string getName() const override { return "Diverge"; }
  void process() override;
  int getGridWidth() const override { return 1; }
  int getGridHeight() const override { return 1; }
};
