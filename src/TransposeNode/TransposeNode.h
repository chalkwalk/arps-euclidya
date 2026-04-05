#pragma once

#include "../GraphNode.h"
#include "../SharedMacroUI.h"

class TransposeNode : public GraphNode {
 public:
  TransposeNode() = default;
  ~TransposeNode() override = default;

  void process() override;
  NodeLayout getLayout() const override;

  std::vector<MacroParam *> getMacroParams() override {
    return {&macroSemitones};
  }

  std::string getName() const override { return "Transpose"; }

  void saveNodeState(juce::XmlElement *xml) override;
  void loadNodeState(juce::XmlElement *xml) override;

  int semitones = 0;
  MacroParam macroSemitones{"Semitones", {}};
};
