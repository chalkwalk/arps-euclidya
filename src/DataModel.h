#pragma once

#include <algorithm>
#include <cstdint>
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

using NoteSequence = std::vector<std::vector<HeldNote>>;
