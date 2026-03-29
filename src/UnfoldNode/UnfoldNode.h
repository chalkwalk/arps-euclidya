#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../GraphNode.h"

class UnfoldNode : public GraphNode {
 public:
  UnfoldNode();
  ~UnfoldNode() override = default;

  void process() override;
  [[nodiscard]] NodeLayout getLayout() const override;

  [[nodiscard]] std::string getName() const override { return "Unfold"; }

  void saveNodeState(juce::XmlElement *xml) override;
  void loadNodeState(juce::XmlElement *xml) override;

  int ordering = 0;  // 0: Ascending, 1: Descending
  int fixedWidth =
      0;  // 0: Variable length, 1: Fixed width (pad/drop to max chord size)
  int noteBias =
      0;  // 0: Bottom, 1: Middle, 2: Top (which notes to keep when dropping)
};
