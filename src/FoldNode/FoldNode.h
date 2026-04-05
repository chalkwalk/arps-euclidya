#pragma once

#include "../GraphNode.h"
#include "../SharedMacroUI.h"

class FoldNode : public GraphNode {
 public:
  FoldNode() = default;
  ~FoldNode() override = default;

  void process() override;
  NodeLayout getLayout() const override;

  std::vector<MacroParam *> getMacroParams() override { return {&macroNValue}; }

  std::string getName() const override { return "Fold"; }

  void saveNodeState(juce::XmlElement *xml) override;
  void loadNodeState(juce::XmlElement *xml) override;

  int nValue = 2;
  MacroParam macroNValue{"Fold N", {}};
  int mode = 0;
};
