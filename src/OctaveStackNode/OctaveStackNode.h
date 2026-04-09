#pragma once

#include "../GraphNode.h"
#include "../SharedMacroUI.h"

class OctaveStackNode : public GraphNode {
 public:
  OctaveStackNode() { addMacroParam(&macroOctaves); }
  std::string getName() const override { return "Octave Stack"; }
  void process() override;
  NodeLayout getLayout() const override;

  void saveNodeState(juce::XmlElement *xml) override;
  void loadNodeState(juce::XmlElement *xml) override;

  int octaves = 1;
  MacroParam macroOctaves{"Octave Stack", {}};
  int uniqueOnly = 1;  // 1 = true, 0 = false
};
