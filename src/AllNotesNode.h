#pragma once

#include "GraphNode.h"
#include <juce_gui_basics/juce_gui_basics.h>

class AllNotesNode : public GraphNode {
public:
  AllNotesNode();
  ~AllNotesNode() override = default;

  void process() override;
  std::unique_ptr<juce::Component>
  createEditorComponent(juce::AudioProcessorValueTreeState &apvts) override;

  std::string getName() const override { return "All Notes"; }

  void saveNodeState(juce::XmlElement *xml) override;
  void loadNodeState(juce::XmlElement *xml) override;

  int baseOctave = 3;
};
