#pragma once

#include "ClockManager.h"
#include "GraphNode.h"
#include "MidiHandler.h"
#include <juce_audio_processors/juce_audio_processors.h>

class MidiOutNode : public GraphNode {
public:
  MidiOutNode(MidiHandler &midiCtx, ClockManager &clockCtx);
  ~MidiOutNode() override = default;

  std::string getName() const override { return "Midi Out"; }

  // Re-caches internally when graph sequence changes
  void process() override;

  // Called on audio thread to populate output buffer
  void generateMidi(juce::MidiBuffer &outputBuffer, int currentParamId);

private:
  MidiHandler &midiHandler;
  ClockManager &clockManager;

  int sequenceIndex = 0;

  // Notes currently playing that need a NoteOff sent later (channel,
  // noteNumber)
  std::vector<std::pair<int, int>> playingNotes;
};
