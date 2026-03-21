#pragma once

#include "../GraphNode.h"
#include <juce_gui_basics/juce_gui_basics.h>

class QuantizerNode : public GraphNode {
public:
  QuantizerNode();
  ~QuantizerNode() override = default;

  void process() override;
  std::unique_ptr<juce::Component>
  createEditorComponent(juce::AudioProcessorValueTreeState &apvts) override;

  std::string getName() const override { return "Quantizer"; }

  void saveNodeState(juce::XmlElement *xml) override;
  void loadNodeState(juce::XmlElement *xml) override;

  int tonality = 0;
  int rootNote = 0;
  int mode = 0;
  int getGridWidth() const override { return 2; }
  int getGridHeight() const override { return 2; }
};
