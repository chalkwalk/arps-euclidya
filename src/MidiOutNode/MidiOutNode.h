#pragma once

#include "../ClockManager.h"
#include "../GraphNode.h"
#include "../MidiHandler.h"
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

  enum class SyncMode {
    Gestural = 0,     // Starts on key, relative phase
    Synchronized = 1, // Interaction snapped to grid, incremental phase
    Deterministic = 2 // Position bound to absolute DAW playhead
  };

  enum class PatternMode {
    Gated = 0,  // Pattern acts as a filter (skips are rests)
    Clocked = 1 // Pattern acts as a melodic map (skips are jumps)
  };

  void saveNodeState(juce::XmlElement *xml) override;
  void loadNodeState(juce::XmlElement *xml) override;

  int getPatternIndex() const { return visualPatternIndex; }
  int getRhythmIndex() const { return visualRhythmIndex; }

  std::vector<bool> getPattern() const;
  std::vector<bool> getRhythm() const;

  NodeLayout getLayout() const override;
  std::unique_ptr<juce::Component> createCustomComponent(
      const juce::String &name,
      juce::AudioProcessorValueTreeState *apvts = nullptr) override;

  void clampParameters();

  std::function<void()> onParameterChanged;

  std::vector<std::pair<juce::String, int *>> getMacroMappings() override {
    return {{"Pattern Steps", &macroPSteps},
            {"Pattern Beats", &macroPBeats},
            {"Pattern Offset", &macroPOffset},
            {"Rhythm Steps", &macroRSteps},
            {"Rhythm Beats", &macroRBeats},
            {"Rhythm Offset", &macroROffset},
            {"Press -> Vel", &macroPressureToVelocity},
            {"Timb -> Vel", &macroTimbreToVelocity}};
  }

  int pSteps = 16;
  int pBeats = 11;
  int pOffset = 0;
  int rSteps = 8;
  int rBeats = 7;
  int rOffset = 0;

  int macroPSteps = -1;
  int macroPBeats = -1;
  int macroPOffset = -1;
  int macroRSteps = -1;
  int macroRBeats = -1;
  int macroROffset = -1;

  int macroPressureToVelocity = -1;
  int macroTimbreToVelocity = -1;

  // Sync & Reset Controls
  SyncMode syncMode = SyncMode::Synchronized;
  PatternMode patternMode = PatternMode::Gated;
  int transportSyncMode = 0;         // DEPRECATED - replaced by syncMode
  bool patternResetOnRelease = true; // Reset sequence + pattern on all-keys-up
  bool rhythmResetOnRelease = true;  // Reset rhythm on all-keys-up
  int clockDivisionIndex = 5;        // Index into division table (default: 1/8)
  bool triplet = false;              // Triplet modifier
  int outputChannel = 1;             // Output MIDI channel (1-16)

  float pressureToVelocity = 0.0f;
  float timbreToVelocity = 0.0f;

private:
  MidiHandler &midiHandler;
  ClockManager &clockManager;
  std::array<std::atomic<float> *, 32> macros;

  int sequenceIndex = 0;
  int patternIndex = 0;
  int rhythmIndex = 0;
  int visualPatternIndex = 0;
  int visualRhythmIndex = 0;

  NoteSequence previousSequence;

  // Timing state
  bool wasHoldingNotes = false; // Detect transition to no notes
  bool syncArmed = true;   // Waiting for first key press or transport start
  bool forceTick = false;  // Fire immediately on next call
  double anchorPpq = 0.0;  // Phase anchor for relative modes
  bool wasPlaying = false; // Detect transport start/stop

  // Per-node tick tracking
  double lastTickPpq = -1.0;

  // Notes currently playing that need a NoteOff sent later (channel,
  // noteNumber)
  std::vector<std::pair<int, int>> playingNotes;
};
