#pragma once

#include "ClockManager.h"
#include "EuclideanMath.h"
#include "GraphNode.h"
#include "MidiHandler.h"
#include <array>
#include <atomic>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

class MidiOutNode : public GraphNode {
public:
  MidiOutNode(MidiHandler &midiCtx, ClockManager &clockCtx,
              std::array<std::atomic<float> *, 32> macrosArray);
  ~MidiOutNode() override = default;

  std::string getName() const override { return "Midi Out Node"; }

  // Re-caches internally when graph sequence changes
  void process() override;

  void generateMidi(juce::MidiBuffer &outputBuffer, int samplePosition);

private:
  MidiHandler &midiHandler;
  ClockManager &clockManager;
  std::array<std::atomic<float> *, 32> macros;

  int sequenceIndex = 0;
  int patternIndex = 0;

  // Notes currently playing that need a NoteOff sent later (channel,
  // noteNumber)
  std::vector<std::pair<int, int>> playingNotes;
};
