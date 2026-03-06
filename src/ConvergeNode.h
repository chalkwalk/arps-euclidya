#pragma once
#include "GraphNode.h"

class ConvergeNode : public GraphNode {
public:
  ConvergeNode() = default;
  std::string getName() const override { return "Converge"; }
  void process() override;
  int getGridWidth() const override { return 1; }
  int getGridHeight() const override { return 1; }
};
