#pragma once

#include "../GraphNode.h"

class RouteNode : public GraphNode {
 public:
  RouteNode() { addMacroParam(&macroRouteDest); }
  ~RouteNode() override = default;

  std::string getName() const override { return "Route"; }
  int getNumInputPorts() const override { return 1; }
  int getNumOutputPorts() const override { return 2; }

  PortType getInputPortType(int /*port*/) const override { return PortType::Agnostic; }
  PortType getOutputPortType(int /*port*/) const override { return PortType::Agnostic; }

  void process() override;
  NodeLayout getLayout() const override;

  void saveNodeState(juce::XmlElement *xml) override;
  void loadNodeState(juce::XmlElement *xml) override;

  int routeDest = 0;
  MacroParam macroRouteDest{"Route", {}};
};
