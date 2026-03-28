#pragma once

#include "../GraphNode.h"
#include "../SharedMacroUI.h"

class FoldNode : public GraphNode {
 public:
  FoldNode(std::array<std::atomic<float> *, 32> &macros);
  ~FoldNode() override = default;

  void process() override;
  NodeLayout getLayout() const override;

  std::vector<std::pair<juce::String, int *>> getMacroMappings() override {
    return {{"Fold N", &macroNValue}};
  }

  std::string getName() const override { return "Fold"; }

  void saveNodeState(juce::XmlElement *xml) override;
  void loadNodeState(juce::XmlElement *xml) override;

  int nValue = 2;
  int macroNValue = -1;
  int mode = 0;

 private:
  std::array<std::atomic<float> *, 32> &macros;
};
