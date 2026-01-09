#include "MidiHandler.h"

void MidiHandler::processMidi(juce::MidiBuffer &midiMessages) {
  std::lock_guard<std::mutex> lock(stateMutex);

  for (const auto metadata : midiMessages) {
    const auto message = metadata.getMessage();
    const int channel = message.getChannel();

    if (message.isNoteOn()) {
      int note = message.getNoteNumber();
      float velocity = message.getVelocity();

      // Check if note is already held (shouldn't happen often, but be safe)
      auto it =
          std::find_if(heldNotes.begin(), heldNotes.end(),
                       [note, channel](const HeldNote &n) {
                         return n.noteNumber == note && n.channel == channel;
                       });

      if (it == heldNotes.end()) {
        heldNotes.push_back({note, channel, velocity});
        isDirty = true;
      }
    } else if (message.isNoteOff()) {
      int note = message.getNoteNumber();

      auto it =
          std::find_if(heldNotes.begin(), heldNotes.end(),
                       [note, channel](const HeldNote &n) {
                         return n.noteNumber == note && n.channel == channel;
                       });

      if (it != heldNotes.end()) {
        heldNotes.erase(it);
        isDirty = true;

        // Clean up MPE state
        mpeXState.erase(note);
        mpeYState.erase(note);
        mpeZState.erase(note);
      }
    } else if (message.isPitchWheel()) {
      // Usually Pitch Bend is per voice channel in MPE
      // For now, simplify and associate it with the active channel mapping
      // Note: True MPE parsing is more complex, routing Channel -> Note
      // Assuming MPE zone routing is handled upstream, or we simply track last
      // played note on this channel
      if (!heldNotes.empty()) {
        // Find note associated with this channel
        auto it = std::find_if(
            heldNotes.begin(), heldNotes.end(),
            [channel](const HeldNote &n) { return n.channel == channel; });

        if (it != heldNotes.end()) {
          float pb =
              message.getPitchWheelValue() / 16383.0f; // 0.0 to 1.0 roughly
          mpeXState[it->noteNumber] = pb;
        }
      }
    } else if (message.isController() && message.getControllerNumber() == 74) {
      if (!heldNotes.empty()) {
        auto it = std::find_if(
            heldNotes.begin(), heldNotes.end(),
            [channel](const HeldNote &n) { return n.channel == channel; });

        if (it != heldNotes.end()) {
          mpeYState[it->noteNumber] = message.getControllerValue() / 127.0f;
        }
      }
    } else if (message.isChannelPressure()) {
      if (!heldNotes.empty()) {
        auto it = std::find_if(
            heldNotes.begin(), heldNotes.end(),
            [channel](const HeldNote &n) { return n.channel == channel; });

        if (it != heldNotes.end()) {
          mpeZState[it->noteNumber] =
              message.getChannelPressureValue() / 127.0f;
        }
      }
    }
  }
}

std::vector<HeldNote> MidiHandler::getHeldNotes() const {
  std::lock_guard<std::mutex> lock(stateMutex);
  return heldNotes;
}

float MidiHandler::getMpeX(int noteNumber) const {
  std::lock_guard<std::mutex> lock(stateMutex);
  auto it = mpeXState.find(noteNumber);
  return it != mpeXState.end() ? it->second : 0.5f; // PB defaults to center
}

float MidiHandler::getMpeY(int noteNumber) const {
  std::lock_guard<std::mutex> lock(stateMutex);
  auto it = mpeYState.find(noteNumber);
  return it != mpeYState.end() ? it->second : 0.0f;
}

float MidiHandler::getMpeZ(int noteNumber) const {
  std::lock_guard<std::mutex> lock(stateMutex);
  auto it = mpeZState.find(noteNumber);
  return it != mpeZState.end() ? it->second : 0.0f;
}

bool MidiHandler::hasChanged() {
  std::lock_guard<std::mutex> lock(stateMutex);
  bool changed = isDirty;
  isDirty = false;
  return changed;
}
