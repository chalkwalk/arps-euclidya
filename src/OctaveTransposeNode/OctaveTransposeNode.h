#pragma once

#include "../GraphNode.h"
#include "../SharedMacroUI.h"

class OctaveTransposeNode : public GraphNode {
 public:
  OctaveTransposeNode() { addMacroParam(&macroOctaves); }
  ~OctaveTransposeNode() override = default;

  void process() override;
  NodeLayout getLayout() const override;

  std::string getName() const override { return "Octave Transpose"; }

  void saveNodeState(juce::XmlElement *xml) override;
  void loadNodeState(juce::XmlElement *xml) override;

  int octaves = 0;
  MacroParam macroOctaves{"Oct Transpose", {}};
};
