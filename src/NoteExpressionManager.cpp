#include "NoteExpressionManager.h"

#include <functional>
#include <set>

NoteExpressionManager::NoteExpressionManager() {
  mpeInstrument.addListener(this);
  juce::MPEZoneLayout layout;
  layout.setLowerZone(15);  // Standard MPE layout: Master Ch 1, Members 2-16
  mpeInstrument.setZoneLayout(layout);
}

NoteExpressionManager::~NoteExpressionManager() {
  mpeInstrument.removeListener(this);
}

void NoteExpressionManager::processMidi(juce::MidiBuffer &midiMessages) {
  std::lock_guard<std::recursive_mutex> lock(stateMutex);

  for (const auto metadata : midiMessages) {
    const auto &message = metadata.getMessage();
    if (message.isNoteOn()) {
      polyAftertouchState[{message.getChannel(), message.getNoteNumber()}] =
          0.0f;
    } else if (message.isChannelPressure()) {
      int ch = message.getChannel();
      if (ch >= 1 && ch <= 16) {
        channelPressureState[(size_t)(ch - 1)] =
            message.getChannelPressureValue() / 127.0f;
      }
    }

    if (ignoreMpeMasterPressure && !mpeInstrument.isLegacyModeEnabled() &&
        message.isChannelPressure() && message.getChannel() == 1) {
      // Skip Master Channel Pressure in MPE mode if workaround active
    } else {
      mpeInstrument.processNextMidiEvent(message);
    }
  }

  updatePressureSmoothing();
}

void NoteExpressionManager::handleMidiMessage(
    const juce::MidiMessage &message) {
  std::lock_guard<std::recursive_mutex> lock(stateMutex);

  if (message.isNoteOn()) {
    polyAftertouchState[{message.getChannel(), message.getNoteNumber()}] = 0.0f;
  } else if (message.isChannelPressure()) {
    int ch = message.getChannel();
    if (ch >= 1 && ch <= 16) {
      channelPressureState[(size_t)(ch - 1)] =
          message.getChannelPressureValue() / 127.0f;
    }
  }

  if (ignoreMpeMasterPressure && !mpeInstrument.isLegacyModeEnabled() &&
      message.isChannelPressure() && message.getChannel() == 1) {
    // Skip Master Channel Pressure in MPE mode if workaround active
  } else {
    mpeInstrument.processNextMidiEvent(message);
  }

  updatePressureSmoothing();
}

void NoteExpressionManager::handleNoteOn(int channel, int noteNumber,
                                         float velocity, int32_t noteID) {
  std::lock_guard<std::recursive_mutex> lock(stateMutex);

  int targetChannel = channel + 1;

  if (noteID != -1) {
    // 1. Try to use the host's preferred channel first (2-16)
    int preferred = channel + 1;
    if (preferred >= 2 && preferred <= 16 && !channelInUse[(size_t)preferred]) {
      targetChannel = preferred;
    } else {
      // 2. If occupied or invalid, find any free channel
      for (int ch = 2; ch <= 16; ++ch) {
        if (!channelInUse[(size_t)ch]) {
          targetChannel = ch;
          break;
        }
      }
    }

    channelInUse[(size_t)targetChannel] = true;
    clapIDToChannel[noteID] = targetChannel;
    channelToSourceChannel[(size_t)targetChannel] = channel + 1;
  }

  auto noteOnVelocity = juce::MPEValue::fromUnsignedFloat(velocity);
  mpeInstrument.noteOn(targetChannel, noteNumber, noteOnVelocity);
  isDirty = true;

  if (noteID != -1) {
    // Track CLAP noteID against the assigned internal channel
    noteIDMap[{targetChannel, noteNumber}] = noteID;
  }
}

void NoteExpressionManager::handleNoteOff(int channel, int noteNumber,
                                          float velocity, int32_t noteID) {
  std::lock_guard<std::recursive_mutex> lock(stateMutex);

  int targetChannel = -1;

  // 1. Try NoteID lookup if valid
  if (noteID != -1) {
    auto it = clapIDToChannel.find(noteID);
    if (it != clapIDToChannel.end()) {
      targetChannel = it->second;
    }
  }

  // 2. Try Pitch-based lookup if ID failed or was missing (CLAP
  // re-channelization safety)
  if (targetChannel == -1) {
    for (int ch = 2; ch <= 16; ++ch) {
      if (channelInUse[(size_t)ch]) {
        juce::MPENote activeNote = mpeInstrument.getNote(ch, noteNumber);
        if (activeNote.isValid() && activeNote.keyState != juce::MPENote::off) {
          targetChannel = ch;
          break;
        }
      }
    }
  }

  // 3. Final fallback: master channel or legacy mapping
  if (targetChannel == -1) {
    targetChannel = channel + 1;
  }

  auto noteOffVelocity = juce::MPEValue::fromUnsignedFloat(velocity);
  mpeInstrument.noteOff(targetChannel, noteNumber, noteOffVelocity);
  isDirty = true;
}

void NoteExpressionManager::handleNoteExpression(int channel, int noteNumber,
                                                 NoteExpressionType type,
                                                 float value, int32_t noteID) {
  juce::ignoreUnused(noteNumber);
  std::lock_guard<std::recursive_mutex> lock(stateMutex);

  int targetChannel = channel + 1;
  if (noteID != -1) {
    auto it = clapIDToChannel.find(noteID);
    if (it != clapIDToChannel.end()) {
      targetChannel = it->second;
    }
  }

  switch (type) {
    case NoteExpressionType::Tuning:
      mpeInstrument.pitchbend(targetChannel,
                              juce::MPEValue::fromUnsignedFloat(value));
      break;
    case NoteExpressionType::Brightness:
      mpeInstrument.timbre(targetChannel,
                           juce::MPEValue::fromUnsignedFloat(value));
      break;
    case NoteExpressionType::Pressure:
      mpeInstrument.pressure(targetChannel,
                             juce::MPEValue::fromUnsignedFloat(value));
      break;
    case NoteExpressionType::Volume:
      break;
  }
}

std::vector<HeldNote> NoteExpressionManager::getHeldNotes(
    int channelFilter) const {
  std::lock_guard<std::recursive_mutex> lock(stateMutex);
  std::vector<HeldNote> notes;
  int count = mpeInstrument.getNumPlayingNotes();

  for (int i = 0; i < count; ++i) {
    const auto &mpeNote = mpeInstrument.getNote(i);

    if (mpeNote.keyState == juce::MPENote::off ||
        mpeNote.keyState == juce::MPENote::sustained) {
      continue;
    }

    int sourceCh = mpeNote.midiChannel;
    if (mpeNote.midiChannel >= 1 && mpeNote.midiChannel <= 16) {
      if (channelInUse[(size_t)mpeNote.midiChannel]) {
        sourceCh = channelToSourceChannel[(size_t)mpeNote.midiChannel];
      }
    }

    if (channelFilter == 0 || sourceCh == channelFilter) {
      HeldNote n;
      n.noteNumber = mpeNote.initialNote;
      n.channel = sourceCh;
      n.velocity = mpeNote.noteOnVelocity.asUnsignedFloat();
      n.mpeX = mpeNote.pitchbend.asSignedFloat();
      n.mpeY = mpeNote.timbre.asUnsignedFloat();

      auto it =
          smoothedPressure.find({mpeNote.midiChannel, mpeNote.initialNote});
      n.mpeZ = (it != smoothedPressure.end()) ? it->second : 0.0f;

      n.sourceNoteNumber = mpeNote.initialNote;
      n.sourceChannel = sourceCh;

      auto idIt = noteIDMap.find({mpeNote.midiChannel, mpeNote.initialNote});
      if (idIt != noteIDMap.end()) {
        n.sourceNoteID = idIt->second;
        n.noteID = idIt->second;
      }

      notes.push_back(n);
    }
  }

  return notes;
}

float NoteExpressionManager::getMpeX(int channel, int noteNumber,
                                     int32_t noteID) const {
  std::lock_guard<std::recursive_mutex> lock(stateMutex);
  int targetChannel = channel;
  if (noteID != -1) {
    auto it = clapIDToChannel.find(noteID);
    if (it != clapIDToChannel.end()) {
      targetChannel = it->second;
    }
  }
  juce::MPENote note = mpeInstrument.getNote(targetChannel, noteNumber);
  if (note.isValid()) {
    return note.pitchbend.asSignedFloat();  // -1.0 to 1.0
  }
  return 0.0f;
}

float NoteExpressionManager::getMpeY(int channel, int noteNumber,
                                     int32_t noteID) const {
  std::lock_guard<std::recursive_mutex> lock(stateMutex);
  int targetChannel = channel;
  if (noteID != -1) {
    auto it = clapIDToChannel.find(noteID);
    if (it != clapIDToChannel.end()) {
      targetChannel = it->second;
    }
  }
  juce::MPENote note = mpeInstrument.getNote(targetChannel, noteNumber);
  if (note.isValid()) {
    return note.timbre.asUnsignedFloat();  // 0.0 to 1.0
  }
  return 0.0f;
}

float NoteExpressionManager::getMpeZ(int channel, int noteNumber,
                                     int32_t noteID) const {
  std::lock_guard<std::recursive_mutex> lock(stateMutex);
  int targetChannel = channel;
  if (noteID != -1) {
    auto it = clapIDToChannel.find(noteID);
    if (it != clapIDToChannel.end()) {
      targetChannel = it->second;
    }
  }
  auto it = smoothedPressure.find({targetChannel, noteNumber});
  if (it != smoothedPressure.end()) {
    return it->second;
  }
  return 0.0f;
}

bool NoteExpressionManager::hasChanged() {
  std::lock_guard<std::recursive_mutex> lock(stateMutex);
  bool changed = isDirty;
  isDirty = false;
  return changed;
}

void NoteExpressionManager::forceDirty() {
  std::lock_guard<std::recursive_mutex> lock(stateMutex);
  isDirty = true;
}

void NoteExpressionManager::clearAllNotes() {
  std::lock_guard<std::recursive_mutex> lock(stateMutex);
  mpeInstrument.releaseAllNotes();
  polyAftertouchState.clear();
  channelPressureState.fill(0.0f);
  smoothedPressure.clear();
  releasedNotePressure.clear();
  noteIDMap.clear();
  clapIDToChannel.clear();
  channelToSourceChannel.fill(0);
  channelInUse.fill(false);
  isDirty = true;
}

void NoteExpressionManager::setLegacyMode(bool shouldEnable) {
  std::lock_guard<std::recursive_mutex> lock(stateMutex);
  if (shouldEnable) {
    // Non-MPE controllers: +/- 2 semitone range, across all 16 channels
    mpeInstrument.enableLegacyMode(2, juce::Range<int>(1, 17));
  } else {
    juce::MPEZoneLayout layout;
    layout.setLowerZone(15);  // Standard MPE layout: Master Ch 1, Members 2-16
    mpeInstrument.setZoneLayout(layout);
  }
}

bool NoteExpressionManager::isLegacyModeEnabled() const {
  std::lock_guard<std::recursive_mutex> lock(stateMutex);
  return mpeInstrument.isLegacyModeEnabled();
}

void NoteExpressionManager::noteAdded(juce::MPENote /*newNote*/) {
  isDirty = true;
}

void NoteExpressionManager::noteReleased(juce::MPENote finishedNote) {
  auto key = std::make_pair(finishedNote.midiChannel, finishedNote.initialNote);
  smoothedPressure.erase(key);

  auto idIt = noteIDMap.find(key);
  if (idIt != noteIDMap.end()) {
    int32_t noteID = idIt->second;
    clapIDToChannel.erase(noteID);
    noteIDMap.erase(idIt);
  }

  if (finishedNote.midiChannel >= 1 && finishedNote.midiChannel <= 16) {
    channelInUse[(size_t)finishedNote.midiChannel] = false;
  }

  isDirty = true;
}

void NoteExpressionManager::updatePressureSmoothing() {
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
        // Fallback: use per-channel pressure or global (Ch 1) pressure
        targetPressure =
            std::max(channelPressureState[(size_t)(ch - 1)],
                     channelPressureState[0]);  // Ch 1 is Master fallback
      }
    }

    auto &current = smoothedPressure[key];
    if (targetPressure > current) {
      current += (targetPressure - current) * kPressureSlewUp;
    } else {
      current += (targetPressure - current) * kPressureSlewDown;
    }
  }

  for (auto it = smoothedPressure.begin(); it != smoothedPressure.end();) {
    if (activeNotes.find(it->first) == activeNotes.end()) {
      it = smoothedPressure.erase(it);
    } else {
      ++it;
    }
  }
}
