#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

#include <array>
#include <atomic>

#include "../ClockManager.h"
#include "../GraphNode.h"
#include "../NoteExpressionManager.h"

class MidiOutNode : public GraphNode {
 public:
  MidiOutNode(NoteExpressionManager &midiCtx, ClockManager &clockCtx);
  ~MidiOutNode() override = default;

  std::string getName() const override { return "Midi Out"; }
  int getNumOutputPorts() const override { return 0; }

  // Re-caches internally when graph sequence changes
  void process() override;

  // Called immediately when UI controls change
  void parameterChanged() override { clampParameters(); }

  void generateOutput(NoteEventCollector &collector, int numSamples,
                      std::atomic<int32_t> &noteIDCounter);

  // Returns a formatted string showing the cycle length in ticks, quarter
  // beats, and bars
  juce::String getCycleLengthInfo() const;

  enum class SyncMode {
    Gestural = 0,       // Starts on key, relative phase
    Synchronized = 1,   // Interaction snapped to grid, incremental phase
    Deterministic = 2,  // Position bound to absolute DAW playhead
    Forgiving = 3  // Like Synchronized but fires immediately if slightly late,
                   // then uses phase-slip convergence to re-align to the grid
  };

  enum class PatternMode {
    Gated = 0,   // Pattern acts as a filter (skips are rests)
    Clocked = 1  // Pattern acts as a melodic map (skips are jumps)
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

  std::vector<MacroParam *> getMacroParams() override {
    return {&macroPSteps,
            &macroPBeats,
            &macroPOffset,
            &macroRSteps,
            &macroRBeats,
            &macroROffset,
            &macroPressureToVelocity,
            &macroTimbreToVelocity,
            &macroHumTiming,
            &macroHumVelocity,
            &macroHumGate};
  }

  int pSteps = 16;
  int pBeats = 11;
  int pOffset = 0;
  int rSteps = 8;
  int rBeats = 7;
  int rOffset = 0;

  MacroParam macroPSteps{"Pattern Steps", {}};
  MacroParam macroPBeats{"Pattern Beats", {}};
  MacroParam macroPOffset{"Pattern Offset", {}};
  MacroParam macroRSteps{"Rhythm Steps", {}};
  MacroParam macroRBeats{"Rhythm Beats", {}};
  MacroParam macroROffset{"Rhythm Offset", {}};

  // Sync & Reset Controls
  SyncMode syncMode = SyncMode::Synchronized;
  PatternMode patternMode = PatternMode::Gated;
  int transportSyncMode = 0;          // DEPRECATED - replaced by syncMode
  bool patternResetOnRelease = true;  // Reset sequence + pattern on all-keys-up
  bool rhythmResetOnRelease = true;   // Reset rhythm on all-keys-up
  int clockDivisionIndex = 5;  // Index into division table (default: 1/8)
  bool triplet = false;        // Triplet modifier
  int outputChannel = 0;       // Output MIDI channel (0-15)

  float pressureToVelocity = 0.0f;
  float timbreToVelocity = 0.0f;

  float humTiming = 0.0f;
  float humVelocity = 0.0f;
  float humGate = 0.0f;

  MacroParam macroPressureToVelocity{"Press -> Vel", {}};
  MacroParam macroTimbreToVelocity{"Timb -> Vel", {}};
  MacroParam macroHumTiming{"Hum Timing", {}};
  MacroParam macroHumVelocity{"Hum Velocity", {}};
  MacroParam macroHumGate{"Hum Gate", {}};

  // UI proxy variables (NodeBlock UI needs integer pointers)
  int ui_syncMode = 1;
  int ui_patternMode = 0;
  int ui_patternResetOnRelease = 1;
  int ui_rhythmResetOnRelease = 1;
  int ui_triplet = 0;

  int ui_pBeatsMin = 1;
  int ui_pBeatsMax = 32;
  int ui_pOffsetMin = -16;
  int ui_pOffsetMax = 16;
  int ui_rBeatsMin = 1;
  int ui_rBeatsMax = 32;
  int ui_rOffsetMin = -16;
  int ui_rOffsetMax = 16;

  std::atomic<bool> lastTickPlayedNote{false};

 private:
  void flushPlayingNotes(NoteEventCollector &collector, int numSamples);

  NoteExpressionManager &noteExpressionManager;
  ClockManager &clockManager;

  int sequenceIndex = 0;
  int patternIndex = 0;
  int rhythmIndex = 0;
  int visualPatternIndex = 0;
  int visualRhythmIndex = 0;

  NoteSequence previousSequence;

  // Timing state
  bool wasHoldingNotes = false;  // Detect transition to no notes
  bool syncArmed = true;    // Waiting for first key press or transport start
  bool forceTick = false;   // Fire immediately on next call
  double anchorPpq = 0.0;   // Phase anchor for relative modes
  bool wasPlaying = false;  // Detect transport start/stop

  // Per-node tick tracking
  double lastTickPpq = -1.0;

  // Forgiving mode phase-slip: fractional division shortening remaining
  // (0.0 = no correction needed, decays each tick via *=0.5)
  double forgivingSlipFraction = 0.0;

  // Notes currently playing that need a NoteOff sent later
  std::vector<NoteInfo> playingNotes;

  juce::Random rng;
};
