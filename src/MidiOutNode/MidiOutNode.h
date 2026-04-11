#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

#include <array>
#include <atomic>
#include <cstdint>

#include "../ClockManager.h"
#include "../GraphNode.h"
#include "../NoteExpressionManager.h"
#include "../Tuning/TuningTable.h"

// Allocates MIDI channels 2-16 (stored as 0-indexed slots 0-14) for
// per-note microtonal pitch bend.  Picks the slot that was released
// earliest; if all slots are still active it steals the one with the
// lowest release-counter (= has been ringing the longest without a
// fresh allocation).
struct MicrotoneChannelAllocator {
  static constexpr int kSlots = 15;

  struct Slot {
    bool active = false;
    int32_t noteID = -1;
    int64_t releaseCounter = 0;
  };

  std::array<Slot, kSlots> slots{};
  int64_t counter = 0;

  // Returns a 0-indexed slot (add 1 to get the value passed to the collector,
  // which adds another 1 for 1-based MIDI, yielding channels 2-16).
  int allocate(int32_t noteID) {
    for (int i = 0; i < kSlots; ++i) {
      if (!slots[(size_t)i].active) {
        assign(i, noteID);
        return i;
      }
    }
    // All active: steal slot released longest ago (lowest counter).
    int oldest = 0;
    for (int i = 1; i < kSlots; ++i) {
      if (slots[(size_t)i].releaseCounter < slots[(size_t)oldest].releaseCounter) {
        oldest = i;
      }
    }
    assign(oldest, noteID);
    return oldest;
  }

  void release(int32_t noteID) {
    for (auto& s : slots) {
      if (s.noteID == noteID) {
        s.active = false;
        s.releaseCounter = ++counter;
        return;
      }
    }
  }

  void reset() {
    slots = {};
    counter = 0;
  }

 private:
  void assign(int i, int32_t noteID) {
    slots[(size_t)i] = {true, noteID, 0};
  }
};

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

  // Microtonality: set by PluginProcessor; nullptr = 12-TET (no pitch bend).
  // Pointer is non-owning; the TuningTable is owned by PluginProcessor.
  void setActiveTuning(const TuningTable* t) {
    activeTuning = t;
    mptAllocator.reset();
  }
  const TuningTable* activeTuning = nullptr;
  MicrotoneChannelAllocator mptAllocator;

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
