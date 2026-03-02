#pragma once

#include "GraphNode.h"

class RouteNode : public GraphNode {
public:
  RouteNode(std::array<std::atomic<float> *, 32> &macros);
  ~RouteNode() override = default;

  std::string getName() const override { return "Route"; }
  int getNumInputPorts() const override { return 1; }
  int getNumOutputPorts() const override { return 2; }

  void process() override;
  std::unique_ptr<juce::Component>
  createEditorComponent(juce::AudioProcessorValueTreeState &apvts) override;

  void saveNodeState(juce::XmlElement *xml) override;
  void loadNodeState(juce::XmlElement *xml) override;

  int routeDest = 0;
  int macroRouteDest = -1;
  std::array<std::atomic<float> *, 32> &macros;
};
