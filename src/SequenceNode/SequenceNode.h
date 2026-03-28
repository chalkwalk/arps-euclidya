#pragma once

#include "../GraphNode.h"

class SequenceNode : public GraphNode {
 public:
  SequenceNode(std::array<std::atomic<float> *, 32> &macros);
  ~SequenceNode() override = default;

  std::string getName() const override { return "Sequence"; }
  int getNumInputPorts() const override { return 0; }
  int getNumOutputPorts() const override { return 1; }

  void process() override;
  NodeLayout getLayout() const override;
  std::unique_ptr<juce::Component> createCustomComponent(
      const juce::String &name,
      juce::AudioProcessorValueTreeState *apvts = nullptr) override;

  void saveNodeState(juce::XmlElement *xml) override;
  void loadNodeState(juce::XmlElement *xml) override;

  int seqLength = 8;
  int macroSeqLength = -1;

  std::vector<std::pair<juce::String, int *>> getMacroMappings() override {
    return {{"Seq Length", &macroSeqLength}};
  }

  // Full MIDI range grid: 128 notes × 16 steps
  bool grid[128][16] = {{false}};

 private:
  std::array<std::atomic<float> *, 32> &macros;
};
