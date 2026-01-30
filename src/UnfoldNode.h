#pragma once

#include "GraphNode.h"
#include <juce_gui_basics/juce_gui_basics.h>

class UnfoldNode : public GraphNode {
public:
  UnfoldNode();
  ~UnfoldNode() override = default;

  void process() override;
  std::unique_ptr<juce::Component>
  createEditorComponent(juce::AudioProcessorValueTreeState &apvts) override;

  std::string getName() const override { return "Unfold"; }

  void saveNodeState(juce::XmlElement *xml) override;
  void loadNodeState(juce::XmlElement *xml) override;

  int ordering = 0; // 0: Ascending, 1: Descending
};
