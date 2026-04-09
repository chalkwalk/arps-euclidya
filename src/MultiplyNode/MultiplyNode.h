#pragma once

#include "../GraphNode.h"

// Repeats each step N times.
// A,B,C with N=3 becomes A,A,A,B,B,B,C,C,C
class MultiplyNode : public GraphNode {
 public:
  MultiplyNode() { addMacroParam(&macroRepeatCount); }
  ~MultiplyNode() override = default;

  std::string getName() const override { return "Multiply"; }

  void process() override;

  NodeLayout getLayout() const override;

  void saveNodeState(juce::XmlElement *xml) override;
  void loadNodeState(juce::XmlElement *xml) override;

  int repeatCount = 2;                            // 1-16
  MacroParam macroRepeatCount{"Multiply N", {}};  // macro mapping
};
