#pragma once

#include "../GraphNode.h"

class ReverseNode : public GraphNode {
 public:
  ReverseNode() = default;
  ~ReverseNode() override = default;

  std::string getName() const override { return "Reverse"; }

  PortType getInputPortType(int /*port*/) const override { return PortType::Agnostic; }
  PortType getOutputPortType(int /*port*/) const override { return PortType::Agnostic; }

  void process() override;
  int getGridWidth() const override { return 1; }
  int getGridHeight() const override { return 1; }
};
