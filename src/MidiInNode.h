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

  void process() override;
  NodeLayout getLayout() const override;

  int channelFilter = 0; // 0 means all channels
  int macroChannelFilter = -1;
  int legacyMode = 0;

  MidiHandler &getMidiHandler() { return midiHandler; }

  void saveNodeState(juce::XmlElement *xml) override;
  void loadNodeState(juce::XmlElement *xml) override;

  std::vector<std::pair<juce::String, int *>> getMacroMappings() override {
    return {{"Channel Filter", &macroChannelFilter}};
  }

private:
  MidiHandler &midiHandler;
  std::array<std::atomic<float> *, 32> macros;
};
