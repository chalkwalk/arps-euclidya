#pragma once

#include "../GraphNode.h"
#include "../SharedMacroUI.h"

class OctaveTransposeNode : public GraphNode {
 public:
  OctaveTransposeNode() = default;
  ~OctaveTransposeNode() override = default;

  void process() override;
  NodeLayout getLayout() const override;

  std::vector<std::pair<juce::String, int *>> getMacroMappings() override {
    return {{"Oct Transpose", &macroOctaves}};
  }

  std::string getName() const override { return "Octave Transpose"; }

  void saveNodeState(juce::XmlElement *xml) override;
  void loadNodeState(juce::XmlElement *xml) override;

  int octaves = 0;
  int macroOctaves = -1;
};
