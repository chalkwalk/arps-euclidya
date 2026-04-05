#pragma once

#include "../GraphNode.h"
#include "../SharedMacroUI.h"

class OctaveStackNode : public GraphNode {
 public:
  OctaveStackNode() = default;
  std::string getName() const override { return "Octave Stack"; }
  void process() override;
  NodeLayout getLayout() const override;

  std::vector<std::pair<juce::String, int *>> getMacroMappings() override {
    return {{"Octave Stack", &macroOctaves}};
  }

  void saveNodeState(juce::XmlElement *xml) override;
  void loadNodeState(juce::XmlElement *xml) override;

  int octaves = 1;
  int macroOctaves = -1;
  int uniqueOnly = 1;  // 1 = true, 0 = false
};
