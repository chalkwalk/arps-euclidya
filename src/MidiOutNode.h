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

  std::string getName() const override { return "Midi Out"; }
  int getNumOutputPorts() const override { return 0; }

  // Re-caches internally when graph sequence changes
  void process() override;

  void generateMidi(juce::MidiBuffer &outputBuffer, int samplePosition);

  // Returns a formatted string showing the cycle length in ticks, quarter
  // beats, and bars
  juce::String getCycleLengthInfo() const;

  void saveNodeState(juce::XmlElement *xml) override;
  void loadNodeState(juce::XmlElement *xml) override;

  std::unique_ptr<juce::Component>
  createEditorComponent(juce::AudioProcessorValueTreeState &apvts) override;

  std::vector<std::pair<juce::String, int *>> getMacroMappings() override {
    return {
        {"Pattern Steps", &macroPSteps},   {"Pattern Beats", &macroPBeats},
        {"Pattern Offset", &macroPOffset}, {"Rhythm Steps", &macroRSteps},
        {"Rhythm Beats", &macroRBeats},    {"Rhythm Offset", &macroROffset}};
  }

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

  // Step 13: Sync & Reset Controls
  int transportSyncMode = 0;         // 0 = Clock Sync, 1 = Key Sync
  bool patternResetOnRelease = true; // Reset sequence + pattern on all-keys-up
  bool rhythmResetOnRelease = true;  // Reset rhythm on all-keys-up
  int clockDivisionIndex = 5;        // Index into division table (default: 1/8)
  bool triplet = false;              // Triplet modifier
  int outputChannel = 1;             // Output MIDI channel (1-16)

private:
  MidiHandler &midiHandler;
  ClockManager &clockManager;
  std::array<std::atomic<float> *, 32> macros;

  int sequenceIndex = 0;
  int patternIndex = 0;
  int rhythmIndex = 0;

  NoteSequence previousSequence;

  // Step 13: Key Sync state
  bool wasHoldingNotes = false;      // Detect transition to no notes
  bool keySyncArmed = true;          // Waiting for first key press
  bool keySyncImmediateTick = false; // Fire immediately on first keypress
  double keySyncStartPpq = 0.0;      // PPQ anchor for key-sync ticks
  double keySyncInternalPhase = 0.0; // Internal phase anchor for free-run

  // Per-node tick tracking
  double lastTickPpq = -1.0;
  double lastTickPhase = -1.0;

  // Notes currently playing that need a NoteOff sent later (channel,
  // noteNumber)
  std::vector<std::pair<int, int>> playingNotes;
};
