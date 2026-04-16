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
// per-note microtonal pitch bend and round-robin output.
// Allocation is round-robin: the inactive slot that was assigned least
// recently is chosen next (true rotation through all 15 channels).
// When all slots are active the oldest allocation is stolen.
struct MicrotoneChannelAllocator {
  static constexpr int kSlots = 15;

  struct Slot {
    bool active = false;
    int32_t noteID = -1;
    int64_t allocateCounter = 0;  // Incremented on each assign; 0 = never used
  };

  std::array<Slot, kSlots> slots{};
  int64_t counter = 0;

  // Returns {slotIndex, stolenNoteID}. stolenNoteID is -1 if no note was active in that slot.
  std::pair<int, int32_t> allocate(int32_t noteID) {
    // Round-robin: pick the inactive slot with the lowest allocateCounter
    // (= least recently used channel), cycling evenly through all slots.
    int best = -1;
    for (int i = 0; i < kSlots; ++i) {
      if (!slots[(size_t)i].active) {
        if (best == -1 ||
            slots[(size_t)i].allocateCounter <
                slots[(size_t)best].allocateCounter) {
          best = i;
        }
      }
    }
    if (best != -1) {
      assign(best, noteID);
      return {best, -1};
    }
    // All active: steal the slot allocated earliest (lowest allocateCounter).
    int oldest = 0;
    for (int i = 1; i < kSlots; ++i) {
      if (slots[(size_t)i].allocateCounter <
          slots[(size_t)oldest].allocateCounter) {
        oldest = i;
      }
    }
    int32_t stolenNoteID = slots[(size_t)oldest].noteID;
    assign(oldest, noteID);
    return {oldest, stolenNoteID};
  }

  void release(int32_t noteID) {
    for (auto& s : slots) {
      if (s.noteID == noteID) {
        s.active = false;
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
    slots[(size_t)i].active = true;
    slots[(size_t)i].noteID = noteID;
    slots[(size_t)i].allocateCounter = ++counter;
  }
};

// Per-CC# state for the CC registry inside MidiOutNode.
// Audio thread reads slewAmount/anchorValue/holdOnRest and writes currentValue/
// targetValue/lastEmitted127.  The UI thread reads all fields and writes the
// user-editable ones.  Both access is allowed without a lock: all fields are
// POD and small enough that individual reads/writes are naturally atomic on
// x86/ARM.  Structural mutations (add/remove lanes) happen only on the audio
// thread inside process().
struct CCLaneState {
  int ccNumber = 0;
  juce::String name;        // display name, editable in UI
  float anchorValue = 0.0f; // 0..1 — emitted on rest in Reset mode
  bool holdOnRest = true;   // true = Hold (silence), false = Reset (→ anchor)
  float slewAmount = 0.0f;  // 0..1  (0 = instant, 1 = ~8 ticks to converge)
  // --- runtime state (audio thread) ---
  float currentValue = 0.0f;
  float targetValue = 0.0f;
  int lastEmitted127 = -1;  // de-dupe: only emit when quantised value changes
};

class MidiOutNode : public GraphNode {
 public:
  MidiOutNode(NoteExpressionManager &midiCtx, ClockManager &clockCtx);
  ~MidiOutNode() override = default;

  std::string getName() const override { return "Midi Out"; }
  int getNumInputPorts() const override { return 2; }
  int getNumOutputPorts() const override { return 0; }

  PortType getInputPortType(int port) const override {
    return (port == 1) ? PortType::CC : PortType::Notes;
  }

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
  bool passExpressions = false;  // forward mpeX/Y/Z as expressions at trigger
  int channelMode = 0;           // 0 = Fixed (outputChannel), 1 = Round-Robin

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
  int ui_passExpressions = 0;

  int ui_pBeatsMin = 1;
  int ui_pBeatsMax = 32;
  int ui_pOffsetMin = -16;
  int ui_pOffsetMax = 16;
  int ui_rBeatsMin = 1;
  int ui_rBeatsMax = 32;
  int ui_rOffsetMin = -16;
  int ui_rOffsetMax = 16;

  std::atomic<bool> lastTickPlayedNote{false};

  // CC lane registry: one entry per unique CC# connected to input port 1.
  // Structural mutations (add/remove) happen on the audio thread in process().
  std::vector<CCLaneState> ccLanes;

 private:
  void flushPlayingNotes(NoteEventCollector &collector, int numSamples);
  void flushCCSlew(NoteEventCollector &collector, int numSamples,
                   double samplesPerTick);

  NoteExpressionManager &noteExpressionManager;
  ClockManager &clockManager;

  int sequenceIndex = 0;
  int patternIndex = 0;
  int rhythmIndex = 0;
  int visualPatternIndex = 0;
  int visualRhythmIndex = 0;
  int ccIndex = 0;

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
