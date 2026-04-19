#pragma once

#include "../GraphNode.h"

// Outputs notes whose pitch appears in both input sequences.
// Per step: highest-velocity version of each matched pitch is emitted,
// with the MpeCondition narrowed to the intersection of both predicates.
// CC events on either input are dropped.
class AndNode : public GraphNode {
 public:
  AndNode() = default;
  ~AndNode() override = default;

  [[nodiscard]] std::string getName() const override { return "And"; }
  [[nodiscard]] int getNumInputPorts() const override { return 2; }
  [[nodiscard]] int getNumOutputPorts() const override { return 1; }

  void process() override;

  [[nodiscard]] NodeLayout getLayout() const override;

  void saveNodeState(juce::XmlElement *xml) override;
  void loadNodeState(juce::XmlElement *xml) override;

  // 0 = Pad (shorter padded with rests), 1 = Truncate (cut to shorter)
  int lengthMode = 0;
};
