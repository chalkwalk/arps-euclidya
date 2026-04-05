#pragma once

#include "../GraphNode.h"

class SwitchNode : public GraphNode {
 public:
  SwitchNode() = default;
  ~SwitchNode() override = default;

  std::string getName() const override { return "Switch"; }
  int getNumInputPorts() const override { return 1; }
  int getNumOutputPorts() const override { return 1; }

  void process() override;
  NodeLayout getLayout() const override;

  void saveNodeState(juce::XmlElement *xml) override;
  void loadNodeState(juce::XmlElement *xml) override;

  std::vector<MacroParam *> getMacroParams() override { return {&macroSwitch}; }

  int switchOn = 1;
  MacroParam macroSwitch{"Switch", {}};
};
