#pragma once

#include "../GraphNode.h"
#include "../SharedMacroUI.h"

class WalkNode : public GraphNode {
 public:
  WalkNode() = default;
  std::string getName() const override { return "Walk"; }
  void process() override;
  NodeLayout getLayout() const override;

  std::vector<std::pair<juce::String, int *>> getMacroMappings() override {
    return {{"Walk Length", &macroWalkLength}, {"Walk Skew", &macroWalkSkew}};
  }

  void saveNodeState(juce::XmlElement *xml) override;
  void loadNodeState(juce::XmlElement *xml) override;

  int walkLength = 16;
  int macroWalkLength = -1;
  int walkSkewInt = 0;  // -100 to 100
  int macroWalkSkew = -1;
};
