#pragma once

#include "../GraphNode.h"

class DiagnosticNode : public GraphNode {
public:
  DiagnosticNode() = default;
  ~DiagnosticNode() override = default;

  std::string getName() const override { return "Diagnostic"; }

  void process() override;

  NodeLayout getLayout() const override;
  std::unique_ptr<juce::Component> createCustomComponent(
      const juce::String &name,
      juce::AudioProcessorValueTreeState *apvts = nullptr) override;

  const NoteSequence &getLatestSequence() const { return latestSequence; }

private:
  NoteSequence latestSequence;
};
