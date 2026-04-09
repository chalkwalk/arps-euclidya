#pragma once

#include "../GraphNode.h"

class ChordNNode : public GraphNode {
 public:
  ChordNNode() { addMacroParam(&macroNValue); }
  ~ChordNNode() override = default;

  std::string getName() const override { return "ChordN"; }
  void process() override;
  NodeLayout getLayout() const override;

  void saveNodeState(juce::XmlElement *xml) override;
  void loadNodeState(juce::XmlElement *xml) override;

  int nValue = 2;  // Default to 2
  MacroParam macroNValue{"Chord N", {}};
};
