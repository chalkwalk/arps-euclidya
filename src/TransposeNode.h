#pragma once

#include "GraphNode.h"
#include "SharedMacroUI.h"

class TransposeNode : public GraphNode {
public:
  TransposeNode(std::array<std::atomic<float> *, 32> &macros);
  ~TransposeNode() override = default;

  void process() override;
  NodeLayout getLayout() const override;

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
};
