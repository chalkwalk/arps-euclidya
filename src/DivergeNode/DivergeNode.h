#pragma once
#include "../GraphNode.h"

class DivergeNode : public GraphNode {
 public:
  DivergeNode() = default;
  std::string getName() const override { return "Diverge"; }

  PortType getInputPortType(int /*port*/) const override { return PortType::Agnostic; }
  PortType getOutputPortType(int /*port*/) const override { return PortType::Agnostic; }

  void process() override;
  int getGridWidth() const override { return 1; }
  int getGridHeight() const override { return 1; }
};
