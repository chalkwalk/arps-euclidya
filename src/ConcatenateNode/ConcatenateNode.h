#pragma once

#include "../GraphNode.h"

// Concatenates two input sequences end-to-end.
// Input 0's sequence is followed by Input 1's sequence.
class ConcatenateNode : public GraphNode {
 public:
  ConcatenateNode() = default;
  ~ConcatenateNode() override = default;

  std::string getName() const override { return "Concatenate"; }
  int getNumInputPorts() const override { return 2; }
  int getNumOutputPorts() const override { return 1; }

  PortType getInputPortType(int /*port*/) const override { return PortType::Agnostic; }
  PortType getOutputPortType(int /*port*/) const override { return PortType::Agnostic; }

  void process() override;
  int getGridWidth() const override { return 1; }
  int getGridHeight() const override { return 1; }
};
