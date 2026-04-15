#pragma once

#include <algorithm>
#include <cstdint>
#include <variant>
#include <vector>

enum class NoteExpressionType : std::uint8_t {
  Volume,
  Pressure,
  Tuning,
  Brightness
};

// Predicate carried on every HeldNote. Defaults to full-range passthrough.
// MpeFilterNode narrows one axis per node; chaining narrows multiple axes.
// Evaluated at playback tick-time in MidiOutNode::generateOutput().
// Transient — never serialized; re-derived from process() on patch load.
struct MpeCondition {
  float xMin = 0.0f, xMax = 1.0f;  // Pitch Bend (normalized 0..1, 0.5=center)
  float yMin = 0.0f, yMax = 1.0f;  // Timbre / CC74
  float zMin = 0.0f, zMax = 1.0f;  // Pressure

  // Lower bound always inclusive (>=).
  // Upper bound inclusive (<=) only when == 1.0 (natural ceiling);
  // otherwise exclusive (<). This ensures a note at any value lands in
  // exactly one branch for any threshold.
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
  [[nodiscard]] static bool checkAxis(float v, float lo, float hi) {
    return v >= lo && (hi >= 1.0f ? v <= hi : v < hi);
  }

  [[nodiscard]] bool passes(float x, float y, float z) const {
    return checkAxis(x, xMin, xMax) && checkAxis(y, yMin, yMax) &&
           checkAxis(z, zMin, zMax);
  }

  // Narrow one axis in-place. Preserves impossible ranges (min > max) —
  // these always fail passes() and become permanent rests.
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
  void intersectX(float lo, float hi) {
    xMin = std::max(xMin, lo);
    xMax = std::min(xMax, hi);
  }
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
  void intersectY(float lo, float hi) {
    yMin = std::max(yMin, lo);
    yMax = std::min(yMax, hi);
  }
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
  void intersectZ(float lo, float hi) {
    zMin = std::max(zMin, lo);
    zMax = std::min(zMax, hi);
  }

  [[nodiscard]] bool isPassThrough() const {
    return xMin == 0.f && xMax == 1.f && yMin == 0.f && yMax == 1.f &&
           zMin == 0.f && zMax == 1.f;
  }

  // Attempt to hull two conditions into one. Succeeds when every axis has a
  // non-empty intersection (ranges touch or overlap: lo1 <= hi2 && lo2 <= hi1).
  // On success, writes the hull to 'out' and returns true.
  // On failure (any axis is truly disjoint) returns false — keep both notes.
  [[nodiscard]] static bool tryHull(const MpeCondition &a, const MpeCondition &b,
                                    MpeCondition &out) {
    if (a.xMin > b.xMax || b.xMin > a.xMax) { return false; }
    if (a.yMin > b.yMax || b.yMin > a.yMax) { return false; }
    if (a.zMin > b.zMax || b.zMin > a.zMax) { return false; }
    out.xMin = std::min(a.xMin, b.xMin);
    out.xMax = std::max(a.xMax, b.xMax);
    out.yMin = std::min(a.yMin, b.yMin);
    out.yMax = std::max(a.yMax, b.yMax);
    out.zMin = std::min(a.zMin, b.zMin);
    out.zMax = std::max(a.zMax, b.zMax);
    return true;
  }
};

struct HeldNote {
  int noteNumber = 0;
  int channel = 1;
  float velocity = 0.0f;
  float mpeX = 0.0f;  // Pitch Bend
  float mpeY = 0.0f;  // CC74 / Timbre
  float mpeZ = 0.0f;  // Pressure

  // Canonical source tracking
  int sourceNoteNumber = -1;
  int sourceChannel = -1;
  int32_t sourceNoteID = -1;  // CLAP specific note identifier

  // Current note identifier (if applicable)
  int32_t noteID = -1;

  // Playback-time predicate. Defaults to full-range passthrough.
  MpeCondition mpeCondition;

  bool operator==(const HeldNote &other) const {
    if (noteID != -1 && other.noteID != -1) {
      return noteID == other.noteID;
    }
    return noteNumber == other.noteNumber && channel == other.channel;
  }
};

struct NoteInfo {
  int channel = 1;
  int noteNumber = 0;
  int32_t noteID = -1;
  int remainingSamples = -1;  // -1 means no scheduled NoteOff
};

class NoteEventCollector {
 public:
  virtual ~NoteEventCollector() = default;
  virtual void addNoteOn(int channel, int noteNumber, float velocity,
                         int sampleOffset, int32_t noteID = -1) = 0;
  virtual void addNoteOff(int channel, int noteNumber, float velocity,
                          int sampleOffset, int32_t noteID = -1) = 0;
  virtual void addNoteExpression(int channel, int noteNumber,
                                 NoteExpressionType expressionType, float value,
                                 int sampleOffset, int32_t noteID = -1) = 0;
};

// ---------------------------------------------------------------------------
// CC event — a single MIDI CC message, normalised value [0, 1].
// ---------------------------------------------------------------------------
struct CCEvent {
  int ccNumber = 0;    // 0..127
  float value  = 0.f;  // normalised 0..1 (converted to 0..127 at MIDI output)
  int channel  = 1;
};

// ---------------------------------------------------------------------------
// A sequence event is either a note (HeldNote) or a CC value (CCEvent).
// Using std::variant keeps the type value-typed and cache-friendly — no heap
// allocation per event.  Helper free functions (asNote / asCC) give safe,
// concise access without boilerplate std::get_if calls at every site.
// ---------------------------------------------------------------------------
using SequenceEvent = std::variant<HeldNote, CCEvent>;

inline const HeldNote* asNote(const SequenceEvent& e) {
  return std::get_if<HeldNote>(&e);
}
inline HeldNote* asNote(SequenceEvent& e) {
  return std::get_if<HeldNote>(&e);
}
inline const CCEvent* asCC(const SequenceEvent& e) {
  return std::get_if<CCEvent>(&e);
}
inline CCEvent* asCC(SequenceEvent& e) {
  return std::get_if<CCEvent>(&e);
}

// A single time-step: zero or more events (empty = rest).
using EventStep     = std::vector<SequenceEvent>;
// A full sequence: one EventStep per time step.
using EventSequence = std::vector<EventStep>;

// Legacy alias — NoteSequence is now a synonym for EventSequence.
// All existing code continues to compile; the type carried inside each step
// is now SequenceEvent instead of HeldNote directly.  Notes-only nodes access
// events via asNote(); the variant conversion from HeldNote is implicit.
using NoteSequence = EventSequence;
