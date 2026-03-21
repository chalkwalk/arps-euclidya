#pragma once

#include "GraphNode.h"

class SelectNode : public GraphNode {
public:
  SelectNode(std::array<std::atomic<float> *, 32> &macros);
  ~SelectNode() override = default;

  std::string getName() const override { return "Select"; }
  int getNumInputPorts() const override { return 2; }
  int getNumOutputPorts() const override { return 1; }

  void process() override;
  NodeLayout getLayout() const override;

  void saveNodeState(juce::XmlElement *xml) override;
  void loadNodeState(juce::XmlElement *xml) override;

  std::vector<std::pair<juce::String, int *>> getMacroMappings() override {
    return {{"Select", &macroSelectSource}};
  }

  int selectSource = 0;
  int macroSelectSource = -1;

private:
  std::array<std::atomic<float> *, 32> &macros;
};
