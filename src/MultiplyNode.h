#pragma once

#include "GraphNode.h"
#include <array>
#include <atomic>

// Repeats each step N times.
// A,B,C with N=3 becomes A,A,A,B,B,B,C,C,C
class MultiplyNode : public GraphNode {
public:
  MultiplyNode(std::array<std::atomic<float> *, 32> macrosArray)
      : macros(macrosArray) {}
  ~MultiplyNode() override = default;

  std::string getName() const override { return "Multiply"; }

  void process() override;

  std::unique_ptr<juce::Component>
  createEditorComponent(juce::AudioProcessorValueTreeState &apvts) override;

  std::vector<std::pair<juce::String, int *>> getMacroMappings() override {
    return {{"Multiply N", &macroRepeatCount}};
  }

  void saveNodeState(juce::XmlElement *xml) override;
  void loadNodeState(juce::XmlElement *xml) override;

  int repeatCount = 2;       // 1-16
  int macroRepeatCount = -1; // macro mapping

private:
  std::array<std::atomic<float> *, 32> macros;
  int getGridWidth() const override { return 2; }
  int getGridHeight() const override { return 1; }
};
