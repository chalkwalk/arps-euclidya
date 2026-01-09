#pragma once

#include "DataModel.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <map>
#include <mutex>
#include <vector>

class MidiHandler {
public:
  MidiHandler() = default;
  ~MidiHandler() = default;

  // Parses incoming MIDI buffer and updates internal state
  void processMidi(juce::MidiBuffer &midiMessages);

  // Returns a copy of the ordered list of notes currently held
  std::vector<HeldNote> getHeldNotes() const;

  // Get instantaneous continuous MPE state for a given note (X, Y, Z)
  float getMpeX(int noteNumber) const;
  float getMpeY(int noteNumber) const;
  float getMpeZ(int noteNumber) const;

  // Checks if the list of held notes has changed since the last check
  bool hasChanged();

private:
  std::vector<HeldNote> heldNotes;

  // Maps noteNumber -> MPE state
  std::map<int, float> mpeXState; // Pitch Bend
  std::map<int, float> mpeYState; // CC74 / Timbre
  std::map<int, float> mpeZState; // Pressure

  bool isDirty = false;
  mutable std::mutex stateMutex;
};
