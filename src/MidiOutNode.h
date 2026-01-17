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

  std::unique_ptr<juce::Component>
  createEditorComponent(juce::AudioProcessorValueTreeState &apvts) override;

  int pSteps = 16;
  int pBeats = 4;
  int pOffset = 16;
  int rSteps = 16;
  int rBeats = 16;
  int rOffset = 16;

  int macroPSteps = -1;
  int macroPBeats = -1;
  int macroPOffset = -1;
  int macroRSteps = -1;
  int macroRBeats = -1;
  int macroROffset = -1;

private:
  MidiHandler &midiHandler;
  ClockManager &clockManager;
  std::array<std::atomic<float> *, 32> macros;

  int sequenceIndex = 0;
  int patternIndex = 0;
  int rhythmIndex = 0;

  NoteSequence previousSequence;

  // Notes currently playing that need a NoteOff sent later (channel,
  // noteNumber)
  std::vector<std::pair<int, int>> playingNotes;
};
