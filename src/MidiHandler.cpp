#include "MidiHandler.h"

#include <set>

MidiHandler::MidiHandler() {
  mpeInstrument.addListener(this);
  juce::MPEZoneLayout layout;
  layout.setLowerZone(15);  // Standard MPE layout: Master Ch 1, Members 2-16
  mpeInstrument.setZoneLayout(layout);
}

MidiHandler::~MidiHandler() { mpeInstrument.removeListener(this); }

void MidiHandler::processMidi(juce::MidiBuffer &midiMessages) {
  std::lock_guard<std::mutex> lock(stateMutex);

  for (const auto metadata : midiMessages) {
    const auto &message = metadata.getMessage();
    if (message.isNoteOn()) {
      polyAftertouchState[{message.getChannel(), message.getNoteNumber()}] =
          0.0f;
    } else if (message.isAftertouch()) {
      polyAftertouchState[{message.getChannel(), message.getNoteNumber()}] =
          message.getAfterTouchValue() / 127.0f;
    } else if (message.isChannelPressure()) {
      int ch = message.getChannel();
      if (ch >= 1 && ch <= 16) {
        channelPressureState[(size_t)(ch - 1)] =
            message.getChannelPressureValue() / 127.0f;
      }
    }
    mpeInstrument.processNextMidiEvent(message);
  }

  updatePressureSmoothing();
}

std::vector<HeldNote> MidiHandler::getHeldNotes(int channelFilter) const {
  std::lock_guard<std::mutex> lock(stateMutex);
  std::vector<HeldNote> notes;
  int count = mpeInstrument.getNumPlayingNotes();

  // MPEInstrument holds notes in order of recent addition (last is newest)
  // We want to return them in the order they were triggered.
  for (int i = 0; i < count; ++i) {
    juce::MPENote mpeNote = mpeInstrument.getNote(i);

    if (channelFilter == 0 || mpeNote.midiChannel == channelFilter) {
      HeldNote n;
      n.noteNumber = mpeNote.initialNote;
      n.channel = mpeNote.midiChannel;
      n.velocity = mpeNote.noteOnVelocity.asUnsignedFloat();
      n.mpeX = mpeNote.pitchbend.asSignedFloat();
      n.mpeY = mpeNote.timbre.asUnsignedFloat();

      auto it =
          smoothedPressure.find({mpeNote.midiChannel, mpeNote.initialNote});
      if (it != smoothedPressure.end()) {
        n.mpeZ = it->second;
      } else {
        auto relIt = releasedNotePressure.find(
            {mpeNote.midiChannel, mpeNote.initialNote});
        if (relIt != releasedNotePressure.end()) {
          n.mpeZ = relIt->second;
        } else {
          n.mpeZ = 0.0f;
        }
      }

      n.sourceNoteNumber = mpeNote.initialNote;
      n.sourceChannel = mpeNote.midiChannel;

      notes.push_back(n);
    }
  }

  return notes;
}

float MidiHandler::getMpeX(int channel, int noteNumber) const {
  std::lock_guard<std::mutex> lock(stateMutex);
  juce::MPENote note = mpeInstrument.getNote(channel, noteNumber);
  if (note.isValid()) {
    return note.pitchbend.asSignedFloat();  // -1.0 to 1.0
  }
  return 0.0f;
}

float MidiHandler::getMpeY(int channel, int noteNumber) const {
  std::lock_guard<std::mutex> lock(stateMutex);
  juce::MPENote note = mpeInstrument.getNote(channel, noteNumber);
  if (note.isValid()) {
    return note.timbre.asUnsignedFloat();  // 0.0 to 1.0
  }
  return 0.0f;
}

float MidiHandler::getMpeZ(int channel, int noteNumber) const {
  std::lock_guard<std::mutex> lock(stateMutex);
  auto it = smoothedPressure.find({channel, noteNumber});
  if (it != smoothedPressure.end()) {
    return it->second;
  }
  auto relIt = releasedNotePressure.find({channel, noteNumber});
  if (relIt != releasedNotePressure.end()) {
    return relIt->second;
  }
  return 0.0f;
}

bool MidiHandler::hasChanged() {
  std::lock_guard<std::mutex> lock(stateMutex);
  bool changed = isDirty;
  isDirty = false;
  return changed;
}

void MidiHandler::forceDirty() {
  std::lock_guard<std::mutex> lock(stateMutex);
  isDirty = true;
}

void MidiHandler::clearAllNotes() {
  std::lock_guard<std::mutex> lock(stateMutex);
  mpeInstrument.releaseAllNotes();
  polyAftertouchState.clear();
  channelPressureState.fill(0.0f);
  smoothedPressure.clear();
  releasedNotePressure.clear();
  isDirty = true;
}

void MidiHandler::setLegacyMode(bool shouldEnable) {
  std::lock_guard<std::mutex> lock(stateMutex);
  if (shouldEnable) {
    // Non-MPE controllers: +/- 2 semitone range, across all 16 channels
    mpeInstrument.enableLegacyMode(2, juce::Range<int>(1, 17));
  } else {
    juce::MPEZoneLayout layout;
    layout.setLowerZone(15);  // Standard MPE layout: Master Ch 1, Members 2-16
    mpeInstrument.setZoneLayout(layout);
  }
}

bool MidiHandler::isLegacyModeEnabled() const {
  std::lock_guard<std::mutex> lock(stateMutex);
  return mpeInstrument.isLegacyModeEnabled();
}

void MidiHandler::noteAdded(juce::MPENote /*newNote*/) {
  // We do NOT need to lock stateMutex here because this callback is fired
  // synchronously *from within* processMidi(), which has already acquired the
  // lock!
  isDirty = true;
}

void MidiHandler::noteReleased(juce::MPENote finishedNote) {
  // Move to released notes right away so we don't drop pressure if processing
  // order changes
  auto key = std::make_pair(finishedNote.midiChannel, finishedNote.initialNote);
  auto it = smoothedPressure.find(key);
  if (it != smoothedPressure.end()) {
    releasedNotePressure[key] = it->second;
    smoothedPressure.erase(it);
  }
  isDirty = true;
}

void MidiHandler::updatePressureSmoothing() {
  int count = mpeInstrument.getNumPlayingNotes();
  std::set<std::pair<int, int>> activeNotes;

  for (int i = 0; i < count; ++i) {
    juce::MPENote mpeNote = mpeInstrument.getNote(i);
    int ch = mpeNote.midiChannel;
    int note = mpeNote.initialNote;
    auto key = std::make_pair(ch, note);
    activeNotes.insert(key);

    float targetPressure = mpeNote.pressure.asUnsignedFloat();

    if (mpeInstrument.isLegacyModeEnabled()) {
      auto patIt = polyAftertouchState.find(key);
      if (patIt != polyAftertouchState.end() && patIt->second > 0.0f) {
        targetPressure = patIt->second;
      } else {
        targetPressure = channelPressureState[(size_t)(ch - 1)];
      }
    }

    auto &current = smoothedPressure[key];
    if (targetPressure > current) {
      current += (targetPressure - current) * kPressureSlewUp;
    } else {
      current += (targetPressure - current) * kPressureSlewDown;
    }
  }

  for (auto it = releasedNotePressure.begin();
       it != releasedNotePressure.end();) {
    float &current = it->second;
    current += (0.0f - current) * kPressureSlewDown;
    if (current < 0.001f) {
      it = releasedNotePressure.erase(it);
    } else {
      ++it;
    }
  }

  for (auto it = smoothedPressure.begin(); it != smoothedPressure.end();) {
    if (activeNotes.find(it->first) == activeNotes.end()) {
      releasedNotePressure[it->first] = it->second;
      it = smoothedPressure.erase(it);
    } else {
      ++it;
    }
  }
}
