#pragma once

#include "../GraphNode.h"

class SelectNode : public GraphNode {
 public:
  SelectNode() { addMacroParam(&macroSelectSource); }
  ~SelectNode() override = default;

  std::string getName() const override { return "Select"; }
  int getNumInputPorts() const override { return 2; }
  int getNumOutputPorts() const override { return 1; }

  PortType getInputPortType(int /*port*/) const override { return PortType::Agnostic; }
  PortType getOutputPortType(int /*port*/) const override { return PortType::Agnostic; }

  void process() override;
  NodeLayout getLayout() const override;

  void saveNodeState(juce::XmlElement *xml) override;
  void loadNodeState(juce::XmlElement *xml) override;

  int selectSource = 0;
  MacroParam macroSelectSource{"Select", {}};
};
