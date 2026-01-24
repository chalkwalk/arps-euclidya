#pragma once
#include "GraphNode.h"

class DivergeNode : public GraphNode {
public:
  DivergeNode() = default;
  std::string getName() const override { return "Diverge"; }
  void process() override;
};
