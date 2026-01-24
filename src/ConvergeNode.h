#pragma once
#include "GraphNode.h"

class ConvergeNode : public GraphNode {
public:
  ConvergeNode() = default;
  std::string getName() const override { return "Converge"; }
  void process() override;
};
