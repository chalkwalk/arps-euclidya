#pragma once

#include "../GraphNode.h"

// Outputs notes whose pitch appears in exactly one of the two input sequences.
// Per step: notes unique to one input pass through with their original
// MpeCondition unchanged.
// CC events on either input are dropped.
class XorNode : public GraphNode {
 public:
  XorNode() = default;
  ~XorNode() override = default;

  [[nodiscard]] std::string getName() const override { return "Xor"; }
  [[nodiscard]] int getNumInputPorts() const override { return 2; }
  [[nodiscard]] int getNumOutputPorts() const override { return 1; }

  void process() override;

  [[nodiscard]] NodeLayout getLayout() const override;

  void saveNodeState(juce::XmlElement *xml) override;
  void loadNodeState(juce::XmlElement *xml) override;

  // 0 = Pad (shorter padded with rests), 1 = Truncate (cut to shorter)
  int lengthMode = 0;
};
