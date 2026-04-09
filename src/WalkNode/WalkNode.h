#pragma once

#include "../GraphNode.h"
#include "../SharedMacroUI.h"

class WalkNode : public GraphNode {
 public:
  WalkNode() { addMacroParam(&macroWalkLength); addMacroParam(&macroWalkSkew); }
  std::string getName() const override { return "Walk"; }
  void process() override;
  NodeLayout getLayout() const override;

  void saveNodeState(juce::XmlElement *xml) override;
  void loadNodeState(juce::XmlElement *xml) override;

  int walkLength = 16;
  MacroParam macroWalkLength{"Walk Length", {}};
  int walkSkewInt = 0;  // -100 to 100
  MacroParam macroWalkSkew{"Walk Skew", {}};
};
