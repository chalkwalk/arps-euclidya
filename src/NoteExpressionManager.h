#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <map>
#include <mutex>

#include "DataModel.h"

class NoteExpressionManager : private juce::MPEInstrument::Listener {
 public:
  NoteExpressionManager();
  ~NoteExpressionManager() override;

  void processMidi(juce::MidiBuffer &midiMessages);
  void handleMidiMessage(const juce::MidiMessage &message);

  // Note: CLAP events generally use 0-based indexing for channels. This class
  // standardizes internally on 1-based channels. When calling these methods
  // from a 0-based source (like CLAP), you MUST add 1 to the channel argument.
  void handleNoteOn(int channel, int noteNumber, float velocity,
                    int32_t noteID = -1);
  void handleNoteOff(int channel, int noteNumber, float velocity,
                     int32_t noteID = -1);
  void handleNoteExpression(int channel, int noteNumber,
                            NoteExpressionType type, float value,
                            int32_t noteID = -1);

  std::vector<HeldNote> getHeldNotes(int channelFilter = 0) const;

  float getMpeX(int channel, int noteNumber, int32_t noteID = -1) const;
  float getMpeY(int channel, int noteNumber, int32_t noteID = -1) const;
  float getMpeZ(int channel, int noteNumber, int32_t noteID = -1) const;

  bool hasChanged();
  void forceDirty();
  void clearAllNotes();

  void setLegacyMode(bool shouldEnable);
  bool isLegacyModeEnabled() const;

  void setIgnoreMpeMasterPressure(bool shouldIgnore) {
    ignoreMpeMasterPressure = shouldIgnore;
  }
  bool getIgnoreMpeMasterPressure() const { return ignoreMpeMasterPressure; }

 private:
  void noteAdded(juce::MPENote newNote) override;
  void noteReleased(juce::MPENote finishedNote) override;

  void updatePressureSmoothing();

  mutable std::recursive_mutex stateMutex;
  juce::MPEInstrument mpeInstrument;

  std::map<std::pair<int, int>, float> polyAftertouchState;
  std::array<float, 16> channelPressureState{0.0f};

  // Smoothing
  std::map<std::pair<int, int>, float> smoothedPressure;
  std::map<std::pair<int, int>, float> releasedNotePressure;
  static constexpr float kPressureSlewUp = 0.4f;
  static constexpr float kPressureSlewDown = 0.05f;

  bool isDirty = false;

  // CLAP noteID tracking and re-channelization
  std::map<int32_t, int> clapIDToChannel;
  std::array<int, 17> channelToSourceChannel{0};
  std::map<std::pair<int, int>, int32_t> noteIDMap;
  std::array<bool, 17> channelInUse{false};

  bool ignoreMpeMasterPressure = false;
};
