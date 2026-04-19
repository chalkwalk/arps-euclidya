#pragma once

#include "../GraphNode.h"

// Outputs the union of notes from both input sequences.
// Per step: pitches present in both inputs emit the highest-velocity note
// with the MpeCondition hulled (union) across both predicates; pitches in
// only one input pass through unchanged.
// CC events on either input are dropped.
class OrNode : public GraphNode {
 public:
  OrNode() = default;
  ~OrNode() override = default;

  [[nodiscard]] std::string getName() const override { return "Or"; }
  [[nodiscard]] int getNumInputPorts() const override { return 2; }
  [[nodiscard]] int getNumOutputPorts() const override { return 1; }

  void process() override;

  [[nodiscard]] NodeLayout getLayout() const override;

  void saveNodeState(juce::XmlElement *xml) override;
  void loadNodeState(juce::XmlElement *xml) override;

  // 0 = Pad (shorter padded with rests), 1 = Truncate (cut to shorter)
  int lengthMode = 0;
};
