#pragma once
#include "../GraphNode.h"
#include "../SharedMacroUI.h"
#include <array>
#include <atomic>

class WalkNode : public GraphNode {
public:
  WalkNode(std::array<std::atomic<float> *, 32> &macros);
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
  int walkSkewInt = 0; // -100 to 100
  int macroWalkSkew = -1;

private:
  std::array<std::atomic<float> *, 32> &macros;
};
