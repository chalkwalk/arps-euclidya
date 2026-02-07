#pragma once

#include "GraphNode.h"
#include "MidiHandler.h"

class MidiInNode : public GraphNode {
public:
  MidiInNode(MidiHandler &handler,
             std::array<std::atomic<float> *, 32> macrosArray);
  ~MidiInNode() override = default;

  std::string getName() const override { return "Midi In"; }
  int getNumInputPorts() const override { return 0; }

  // Reads from MidiHandler and generates sequence cache
  void process() override;

  int channelFilter = 0; // 0 means all channels
  int macroChannelFilter = -1;
  bool legacyMode = false;

  MidiHandler &getMidiHandler() { return midiHandler; }

  void saveNodeState(juce::XmlElement *xml) override;
  void loadNodeState(juce::XmlElement *xml) override;
  std::unique_ptr<juce::Component>
  createEditorComponent(juce::AudioProcessorValueTreeState &apvts) override;

private:
  MidiHandler &midiHandler;
  std::array<std::atomic<float> *, 32> macros;
};
