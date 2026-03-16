#pragma once

#include "GraphNode.h"

class DiagnosticNode : public GraphNode {
public:
  DiagnosticNode() = default;
  ~DiagnosticNode() override = default;

  std::string getName() const override { return "Diagnostic"; }

  void process() override;
  int getGridWidth() const override { return 2; }
  int getGridHeight() const override { return 2; }

  std::unique_ptr<juce::Component>
  createEditorComponent(juce::AudioProcessorValueTreeState &apvts) override;

  const NoteSequence &getLatestSequence() const { return latestSequence; }

private:
  NoteSequence latestSequence;
};
