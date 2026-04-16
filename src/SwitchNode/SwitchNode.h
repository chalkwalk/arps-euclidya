#pragma once

#include "../GraphNode.h"

class SwitchNode : public GraphNode {
 public:
  SwitchNode() { addMacroParam(&macroSwitch); }
  ~SwitchNode() override = default;

  std::string getName() const override { return "Switch"; }
  int getNumInputPorts() const override { return 1; }
  int getNumOutputPorts() const override { return 1; }

  PortType getInputPortType(int /*port*/) const override { return PortType::Agnostic; }
  PortType getOutputPortType(int /*port*/) const override { return PortType::Agnostic; }

  void process() override;
  NodeLayout getLayout() const override;

  void saveNodeState(juce::XmlElement *xml) override;
  void loadNodeState(juce::XmlElement *xml) override;

  int switchOn = 1;
  MacroParam macroSwitch{"Switch", {}};
};
