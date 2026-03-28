#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../GraphNode.h"

class QuantizerNode : public GraphNode {
 public:
  QuantizerNode();
  ~QuantizerNode() override = default;

  void process() override;
  NodeLayout getLayout() const override;

  std::string getName() const override { return "Quantizer"; }

  void saveNodeState(juce::XmlElement *xml) override;
  void loadNodeState(juce::XmlElement *xml) override;

  int tonality = 0;
  int rootNote = 0;
  int mode = 0;
  int restOnDrop = 1;
};
