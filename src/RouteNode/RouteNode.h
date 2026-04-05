#pragma once

#include "../GraphNode.h"

class RouteNode : public GraphNode {
 public:
  RouteNode() = default;
  ~RouteNode() override = default;

  std::string getName() const override { return "Route"; }
  int getNumInputPorts() const override { return 1; }
  int getNumOutputPorts() const override { return 2; }

  void process() override;
  NodeLayout getLayout() const override;

  void saveNodeState(juce::XmlElement *xml) override;
  void loadNodeState(juce::XmlElement *xml) override;

  std::vector<std::pair<juce::String, int *>> getMacroMappings() override {
    return {{"Route", &macroRouteDest}};
  }

  int routeDest = 0;
  int macroRouteDest = -1;
};
