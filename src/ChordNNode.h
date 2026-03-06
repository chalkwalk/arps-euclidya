#pragma once
#include "GraphNode.h"
#include "SharedMacroUI.h"
#include <array>
#include <atomic>

class ChordNNodeEditor;

class ChordNNode : public GraphNode {
public:
  ChordNNode(std::array<std::atomic<float> *, 32> &macros);
  std::string getName() const override { return "ChordN"; }
  void process() override;
  std::unique_ptr<juce::Component>
  createEditorComponent(juce::AudioProcessorValueTreeState &apvts) override;

  std::vector<std::pair<juce::String, int *>> getMacroMappings() override {
    return {{"Chord N", &macroNValue}};
  }

  void saveNodeState(juce::XmlElement *xml) override;
  void loadNodeState(juce::XmlElement *xml) override;

  int nValue = 2; // Default to 2
  int macroNValue = -1;

private:
  std::array<std::atomic<float> *, 32> &macros;
  friend class ChordNNodeEditor;
  int getGridWidth() const override { return 2; }
  int getGridHeight() const override { return 1; }
};
