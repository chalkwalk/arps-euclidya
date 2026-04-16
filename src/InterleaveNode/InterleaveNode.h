#pragma once

#include "../GraphNode.h"

// Interleaves steps from two input sequences: A[0], B[0], A[1], B[1], …
// The shorter input is padded with rests to match the longer.
// Output length = 2 × max(len(A), len(B)).
class InterleaveNode : public GraphNode {
 public:
  InterleaveNode() = default;
  ~InterleaveNode() override = default;

  [[nodiscard]] std::string getName() const override { return "Interleave"; }
  [[nodiscard]] int getNumInputPorts() const override { return 2; }
  [[nodiscard]] int getNumOutputPorts() const override { return 1; }

  [[nodiscard]] PortType getInputPortType(int /*port*/) const override { return PortType::Agnostic; }
  [[nodiscard]] PortType getOutputPortType(int /*port*/) const override { return PortType::Agnostic; }

  void process() override;

  [[nodiscard]] NodeLayout getLayout() const override;

  void saveNodeState(juce::XmlElement * /*xml*/) override {}
  void loadNodeState(juce::XmlElement * /*xml*/) override {}
};
