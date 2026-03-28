#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../GraphNode.h"

class UnfoldNode : public GraphNode {
 public:
  UnfoldNode();
  ~UnfoldNode() override = default;

  void process() override;
  NodeLayout getLayout() const override;

  std::string getName() const override { return "Unfold"; }

  void saveNodeState(juce::XmlElement *xml) override;
  void loadNodeState(juce::XmlElement *xml) override;

  int ordering = 0;  // 0: Ascending, 1: Descending
};
