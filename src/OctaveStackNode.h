#pragma once
#include "GraphNode.h"
#include "SharedMacroUI.h"
#include <array>
#include <atomic>

class OctaveStackNodeEditor;

class OctaveStackNode : public GraphNode {
public:
  OctaveStackNode(std::array<std::atomic<float> *, 32> &macros);
  std::string getName() const override { return "Octave Stack"; }
  void process() override;
  std::unique_ptr<juce::Component>
  createEditorComponent(juce::AudioProcessorValueTreeState &apvts) override;

  std::vector<std::pair<juce::String, int *>> getMacroMappings() override {
    return {{"Octave Stack", &macroOctaves}};
  }

  void saveNodeState(juce::XmlElement *xml) override;
  void loadNodeState(juce::XmlElement *xml) override;

  int octaves = 1;
  int macroOctaves = -1;
  bool uniqueOnly = true;

private:
  std::array<std::atomic<float> *, 32> &macros;
  friend class OctaveStackNodeEditor;
  int getGridWidth() const override { return 1; }
  int getGridHeight() const override { return 1; }
};
