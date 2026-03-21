#pragma once

#include "../GraphNode.h"

// Splits each chord in the sequence into two parts.
// Mode 0 (Top): highest note goes to Output 1, rest to Output 0.
// Mode 1 (Bottom): lowest note goes to Output 0, rest to Output 1.
// Empty steps produce rests (empty chords) on both outputs.
class ChordSplitNode : public GraphNode {
public:
  ChordSplitNode() = default;
  ~ChordSplitNode() override = default;

  std::string getName() const override { return "Chord Split"; }
  int getNumInputPorts() const override { return 1; }
  int getNumOutputPorts() const override { return 2; }

  void process() override;

  std::unique_ptr<juce::Component>
  createEditorComponent(juce::AudioProcessorValueTreeState &apvts) override;

  void saveNodeState(juce::XmlElement *xml) override;
  void loadNodeState(juce::XmlElement *xml) override;

  int splitMode = 0; // 0 = Top (highest soloed), 1 = Bottom (lowest soloed)
  int getGridWidth() const override { return 1; }
  int getGridHeight() const override { return 1; }
};
