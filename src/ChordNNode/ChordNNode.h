#pragma once

#include "../GraphNode.h"

class ChordNNode : public GraphNode {
 public:
  ChordNNode() = default;
  ~ChordNNode() override = default;

  std::string getName() const override { return "ChordN"; }
  void process() override;
  NodeLayout getLayout() const override;

  std::vector<std::pair<juce::String, int *>> getMacroMappings() override {
    return {{"Chord N", &macroNValue}};
  }

  void saveNodeState(juce::XmlElement *xml) override;
  void loadNodeState(juce::XmlElement *xml) override;

  int nValue = 2;  // Default to 2
  int macroNValue = -1;
};
