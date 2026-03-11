#pragma once

#include "GraphNode.h"
#include "SharedMacroUI.h"

class TransposeNode : public GraphNode {
public:
  TransposeNode(std::array<std::atomic<float> *, 32> &macros);
  ~TransposeNode() override = default;

  void process() override;
  std::unique_ptr<juce::Component>
  createEditorComponent(juce::AudioProcessorValueTreeState &apvts) override;

  std::vector<std::pair<juce::String, int *>> getMacroMappings() override {
    return {{"Transpose", &macroSemitones}};
  }

  std::string getName() const override { return "Transpose"; }

  void saveNodeState(juce::XmlElement *xml) override;
  void loadNodeState(juce::XmlElement *xml) override;

  int semitones = 0;
  int macroSemitones = -1;

private:
  std::array<std::atomic<float> *, 32> &macros;
  int getGridWidth() const override { return 2; }
  int getGridHeight() const override { return 2; }
};
