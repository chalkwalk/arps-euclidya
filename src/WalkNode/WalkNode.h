#pragma once
#include "../GraphNode.h"
#include "../SharedMacroUI.h"
#include <array>
#include <atomic>

class WalkNodeEditor;

class WalkNode : public GraphNode {
public:
  WalkNode(std::array<std::atomic<float> *, 32> &macros);
  std::string getName() const override { return "Walk"; }
  void process() override;
  std::unique_ptr<juce::Component>
  createEditorComponent(juce::AudioProcessorValueTreeState &apvts) override;

  std::vector<std::pair<juce::String, int *>> getMacroMappings() override {
    return {{"Walk Length", &macroWalkLength}, {"Walk Skew", &macroWalkSkew}};
  }

  void saveNodeState(juce::XmlElement *xml) override;
  void loadNodeState(juce::XmlElement *xml) override;

  int walkLength = 16;
  int macroWalkLength = -1;
  float walkSkew = 0.0f; // -1.0 to 1.0
  int macroWalkSkew = -1;

private:
  std::array<std::atomic<float> *, 32> &macros;
  friend class WalkNodeEditor;
  int getGridWidth() const override { return 2; }
  int getGridHeight() const override { return 2; }
};
