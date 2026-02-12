#pragma once

#include "GraphNode.h"
#include "SharedMacroUI.h"

class OctaveTransposeNode : public GraphNode {
public:
  OctaveTransposeNode(std::array<std::atomic<float> *, 32> &macros);
  ~OctaveTransposeNode() override = default;

  void process() override;
  std::unique_ptr<juce::Component>
  createEditorComponent(juce::AudioProcessorValueTreeState &apvts) override;

  std::vector<std::pair<juce::String, int *>> getMacroMappings() override {
    return {{"Oct Transpose", &macroOctaves}};
  }

  std::string getName() const override { return "Octave Transpose"; }

  void saveNodeState(juce::XmlElement *xml) override;
  void loadNodeState(juce::XmlElement *xml) override;

  int octaves = 0;
  int macroOctaves = -1;

private:
  std::array<std::atomic<float> *, 32> &macros;
};
