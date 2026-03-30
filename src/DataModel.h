#pragma once

#include <cstdint>
#include <vector>

enum class NoteExpressionType : std::uint8_t {
  Volume,
  Pressure,
  Tuning,
  Brightness
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
