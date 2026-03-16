#include "MidiHandler.h"

MidiHandler::MidiHandler() {
  mpeInstrument.addListener(this);
  juce::MPEZoneLayout layout;
  layout.setLowerZone(15); // Standard MPE layout: Master Ch 1, Members 2-16
  mpeInstrument.setZoneLayout(layout);
}

MidiHandler::~MidiHandler() { mpeInstrument.removeListener(this); }

void MidiHandler::processMidi(juce::MidiBuffer &midiMessages) {
  std::lock_guard<std::mutex> lock(stateMutex);

  for (const auto metadata : midiMessages) {
    mpeInstrument.processNextMidiEvent(metadata.getMessage());
  }
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
      n.mpeZ = mpeNote.pressure.asUnsignedFloat();

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
    return note.pitchbend.asSignedFloat(); // -1.0 to 1.0
  }
  return 0.0f;
}

float MidiHandler::getMpeY(int channel, int noteNumber) const {
  std::lock_guard<std::mutex> lock(stateMutex);
  juce::MPENote note = mpeInstrument.getNote(channel, noteNumber);
  if (note.isValid()) {
    return note.timbre.asUnsignedFloat(); // 0.0 to 1.0
  }
  return 0.0f;
}

float MidiHandler::getMpeZ(int channel, int noteNumber) const {
  std::lock_guard<std::mutex> lock(stateMutex);
  juce::MPENote note = mpeInstrument.getNote(channel, noteNumber);
  if (note.isValid()) {
    return note.pressure.asUnsignedFloat(); // 0.0 to 1.0
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
  isDirty = true;
}

void MidiHandler::setLegacyMode(bool shouldEnable) {
  std::lock_guard<std::mutex> lock(stateMutex);
  if (shouldEnable) {
    // Non-MPE controllers: +/- 2 semitone range, across all 16 channels
    mpeInstrument.enableLegacyMode(2, juce::Range<int>(1, 17));
  } else {
    juce::MPEZoneLayout layout;
    layout.setLowerZone(15); // Standard MPE layout: Master Ch 1, Members 2-16
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

void MidiHandler::noteReleased(juce::MPENote /*finishedNote*/) {
  isDirty = true;
}
