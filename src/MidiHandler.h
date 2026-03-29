#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <map>
#include <mutex>
#include <vector>

#include "DataModel.h"

class MidiHandler : public juce::MPEInstrument::Listener {
 public:
  MidiHandler();
  ~MidiHandler() override;

  // Parses incoming MIDI buffer and updates internal state
  void processMidi(juce::MidiBuffer &midiMessages);

  // Returns a copy of the ordered list of notes currently held on the given
  // channel (0 = all)
  std::vector<HeldNote> getHeldNotes(int channelFilter = 0) const;

  // Get instantaneous continuous MPE state for a given note (X, Y, Z)
  float getMpeX(int channel, int noteNumber) const;
  float getMpeY(int channel, int noteNumber) const;
  float getMpeZ(int channel, int noteNumber) const;

  // Checks if the list of held notes has structurally changed (keys
  // added/removed)
  bool hasChanged();
  void forceDirty();
  void clearAllNotes();

  // Toggles the MPE instrument between strict MPE Zones and legacy
  // compatibility mode
  void setLegacyMode(bool shouldEnable);
  bool isLegacyModeEnabled() const;

  void setIgnoreMpeMasterPressure(bool shouldIgnore) {
    ignoreMpeMasterPressure = shouldIgnore;
  }
  bool getIgnoreMpeMasterPressure() const { return ignoreMpeMasterPressure; }

 private:
  // MPEInstrument::Listener overrides
  void noteAdded(juce::MPENote newNote) override;
  void noteReleased(juce::MPENote finishedNote) override;

  void updatePressureSmoothing();

  juce::MPEInstrument mpeInstrument;
  bool isDirty = false;
  mutable std::mutex stateMutex;

  // Manual poly aftertouch tracking (JUCE MPEInstrument ignores it in legacy
  // mode)
  std::map<std::pair<int, int>, float> polyAftertouchState;

  // Per-channel pressure tracking for non-MPE global application
  std::array<float, 16> channelPressureState{};

  // Slew-smoothed pressure per note (key = {channel, noteNumber})
  std::map<std::pair<int, int>, float> smoothedPressure;
  static constexpr float kPressureSlewUp = 0.15f;    // attack smoothing
  static constexpr float kPressureSlewDown = 0.08f;  // release smoothing

  // Track released notes for pressure tail
  std::map<std::pair<int, int>, float> releasedNotePressure;

  bool ignoreMpeMasterPressure = false;
};
