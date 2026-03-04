#pragma once

#include "GraphNode.h"

class SwitchNode : public GraphNode {
public:
  SwitchNode(std::array<std::atomic<float> *, 32> &macros);
  ~SwitchNode() override = default;

  std::string getName() const override { return "Switch"; }
  int getNumInputPorts() const override { return 1; }
  int getNumOutputPorts() const override { return 1; }

  void process() override;
  std::unique_ptr<juce::Component>
  createEditorComponent(juce::AudioProcessorValueTreeState &apvts) override;

  void saveNodeState(juce::XmlElement *xml) override;
  void loadNodeState(juce::XmlElement *xml) override;

  std::vector<std::pair<juce::String, int *>> getMacroMappings() override {
    return {{"Switch", &macroSwitch}};
  }

  bool switchOn = true;
  int macroSwitch = -1;
  std::array<std::atomic<float> *, 32> &macros;
};
