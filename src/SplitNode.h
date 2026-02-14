#pragma once

#include "GraphNode.h"

// Splits a sequence into two halves.
// Mode 0: First half / Second half
// Mode 1: Odd-indexed steps / Even-indexed steps
// Mode 2: Percentage split (controlled by splitPoint)
class SplitNode : public GraphNode {
public:
  SplitNode() = default;
  ~SplitNode() override = default;

  std::string getName() const override { return "Split"; }
  int getNumInputPorts() const override { return 1; }
  int getNumOutputPorts() const override { return 2; }

  void process() override;

  std::unique_ptr<juce::Component>
  createEditorComponent(juce::AudioProcessorValueTreeState &apvts) override;

  void saveNodeState(juce::XmlElement *xml) override;
  void loadNodeState(juce::XmlElement *xml) override;

  int splitMode = 0;     // 0=first/last, 1=odd/even, 2=percentage
  int splitPercent = 50; // Used in percentage mode (1-99)
};
