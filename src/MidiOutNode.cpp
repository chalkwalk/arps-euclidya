#include "MidiOutNode.h"

MidiOutNode::MidiOutNode(MidiHandler &midiCtx, ClockManager &clockCtx,
                         std::array<std::atomic<float> *, 32> macrosArray)
    : midiHandler(midiCtx), clockManager(clockCtx), macros(macrosArray) {}

namespace {
bool stepsAreEqual(const std::vector<HeldNote> &a,
                   const std::vector<HeldNote> &b) {
  if (a.size() != b.size())
    return false;
  for (size_t i = 0; i < a.size(); ++i) {
    if (!(a[i] == b[i]))
      return false;
  }
  return true;
}

int findClosestNoteIndex(const std::vector<HeldNote> &stepToFind,
                         const NoteSequence &oldSequence,
                         const NoteSequence &newSequence, int previousIndex) {
  if (newSequence.empty())
    return 0;

  int nL = newSequence.size();

  for (int i = 0; i < nL; ++i) {
    int cI = (previousIndex + i) % nL;
    if (stepsAreEqual(newSequence[cI], stepToFind))
      return cI;
  }

  for (int i = 0; i < nL; ++i) {
    if (stepsAreEqual(newSequence[i], stepToFind))
      return i;
  }

  int oL = std::max(1, (int)oldSequence.size());
  return ((previousIndex * nL) / oL) % nL;
}

// Clock division table: PPQ values for each division
// Index: 0=4/1, 1=2/1, 2=1/1, 3=1/2, 4=1/4, 5=1/8, 6=1/16, 7=1/32
constexpr double DIVISIONS[] = {16.0, 8.0, 4.0, 2.0, 1.0, 0.5, 0.25, 0.125};
constexpr int NUM_DIVISIONS = 8;

} // namespace

void MidiOutNode::process() {
  auto it = inputSequences.find(0);
  if (it != inputSequences.end()) {
    const NoteSequence &newSequence = it->second;
    int numSteps = newSequence.size();

    if (numSteps == 0) {
      sequenceIndex = 0;
    } else if (!previousSequence.empty() &&
               sequenceIndex < (int)previousSequence.size()) {
      auto stepToFind = previousSequence[sequenceIndex];
      sequenceIndex = findClosestNoteIndex(stepToFind, previousSequence,
                                           newSequence, sequenceIndex);
    } else {
      if (sequenceIndex >= numSteps) {
        sequenceIndex = 0;
      }
    }

    previousSequence = newSequence;
  }
}

void MidiOutNode::generateMidi(juce::MidiBuffer &outputBuffer,
                               int samplePosition) {
  // --- Reset-on-release detection ---
  auto it0 = inputSequences.find(0);
  bool holdingNotes = (it0 != inputSequences.end() && !it0->second.empty());

  if (wasHoldingNotes && !holdingNotes) {
    // All keys just released
    if (patternResetOnRelease) {
      sequenceIndex = 0;
      patternIndex = 0;
    }
    if (rhythmResetOnRelease) {
      rhythmIndex = 0;
    }
    if (transportSyncMode == 1) {
      keySyncArmed = true;
    }
  }

  // Get the unified cumulative PPQ (always incrementing, host or free-running)
  double currentPpq = clockManager.getCumulativePpq();

  // Detect first keypress for Key Sync arming
  if (!wasHoldingNotes && holdingNotes && transportSyncMode == 1 &&
      keySyncArmed) {
    keySyncArmed = false;
    keySyncImmediateTick = true;
    keySyncStartPpq = currentPpq; // Anchor "the 1" to this moment
  }

  wasHoldingNotes = holdingNotes;

  // --- Compute the effective division in PPQ ---
  int divIdx = std::clamp(clockDivisionIndex, 0, NUM_DIVISIONS - 1);
  double division = DIVISIONS[divIdx];
  if (triplet) {
    division *= (2.0 / 3.0);
  }

  // --- Determine if this block is a tick ---
  bool isTick = false;

  if (keySyncImmediateTick) {
    // Key Sync: fire immediately on first keypress — this IS the "1"
    isTick = true;
    keySyncImmediateTick = false;
    lastTickPpq = currentPpq; // Initialize for subsequent detection
  } else if (lastTickPpq >= 0.0) {
    // Normal tick detection using PPQ boundary crossings
    double effectiveCurrent, effectivePrev;

    if (transportSyncMode == 0) {
      // Clock Sync: ticks on the absolute grid
      effectiveCurrent = currentPpq;
      effectivePrev = lastTickPpq;
    } else {
      // Key Sync: ticks relative to key-press anchor
      effectiveCurrent = currentPpq - keySyncStartPpq;
      effectivePrev = lastTickPpq - keySyncStartPpq;
    }

    double prevTick = std::floor(effectivePrev / division);
    double currTick = std::floor(effectiveCurrent / division);
    if (currTick > prevTick) {
      isTick = true;
    }
  }

  lastTickPpq = currentPpq;

  if (isTick) {
    for (const auto &note : playingNotes) {
      outputBuffer.addEvent(
          juce::MidiMessage::noteOff(outputChannel, note.second),
          samplePosition);
    }
    playingNotes.clear();
  }

  auto it = inputSequences.find(0);
  if (it == inputSequences.end() || it->second.empty())
    return;

  if (isTick) {
    const auto &sequence = it->second;

    // --- 1. RHYTHM LAYER (Beat vs Rest) ---
    int actualRSteps =
        macroRSteps != -1 && macros[macroRSteps] != nullptr
            ? 1 + (int)std::round(macros[macroRSteps]->load() * 31.0f)
            : rSteps;
    int actualRBeats =
        macroRBeats != -1 && macros[macroRBeats] != nullptr
            ? 1 + (int)std::round(macros[macroRBeats]->load() * 31.0f)
            : rBeats;
    int actualROffset =
        macroROffset != -1 && macros[macroROffset] != nullptr
            ? (int)std::round(macros[macroROffset]->load() * 32.0f)
            : rOffset;

    std::vector<bool> rhythmPattern = EuclideanMath::generatePattern(
        actualRSteps, actualRBeats, actualROffset);

    if (rhythmPattern.empty())
      return;

    if (rhythmIndex >= (int)rhythmPattern.size()) {
      rhythmIndex = 0;
    }

    bool isRhythmBeat = rhythmPattern[rhythmIndex];
    rhythmIndex = (int)((rhythmIndex + 1) % rhythmPattern.size());

    if (!isRhythmBeat) {
      return;
    }

    // --- 2. PATTERN LAYER (Note vs Skip) ---
    int actualPSteps =
        macroPSteps != -1 && macros[macroPSteps] != nullptr
            ? 1 + (int)std::round(macros[macroPSteps]->load() * 31.0f)
            : pSteps;
    int actualPBeats =
        macroPBeats != -1 && macros[macroPBeats] != nullptr
            ? 1 + (int)std::round(macros[macroPBeats]->load() * 31.0f)
            : pBeats;
    int actualPOffset =
        macroPOffset != -1 && macros[macroPOffset] != nullptr
            ? (int)std::round(macros[macroPOffset]->load() * 32.0f)
            : pOffset;

    std::vector<bool> pattern = EuclideanMath::generatePattern(
        actualPSteps, actualPBeats, actualPOffset);

    if (pattern.empty())
      return;

    if (patternIndex >= (int)pattern.size()) {
      patternIndex = 0;
    }

    size_t skipsProcessed = 0;
    while (!pattern[patternIndex] && skipsProcessed < pattern.size()) {
      patternIndex = (int)((patternIndex + 1) % pattern.size());
      sequenceIndex = (int)((sequenceIndex + 1) % sequence.size());
      skipsProcessed++;
    }

    if (pattern[patternIndex]) {
      const auto &step = sequence[sequenceIndex];

      patternIndex = (int)((patternIndex + 1) % pattern.size());
      sequenceIndex = (int)((sequenceIndex + 1) % sequence.size());

      for (const HeldNote &noteTrigger : step) {
        float currentPressure =
            midiHandler.getMpeZ(noteTrigger.channel, noteTrigger.noteNumber);
        float finalVelocity = std::clamp(
            noteTrigger.velocity + (currentPressure * 0.5f), 0.0f, 1.0f);

        outputBuffer.addEvent(juce::MidiMessage::noteOn(outputChannel,
                                                        noteTrigger.noteNumber,
                                                        finalVelocity),
                              samplePosition);

        playingNotes.push_back({outputChannel, noteTrigger.noteNumber});
      }
    }
  }
}

void MidiOutNode::saveNodeState(juce::XmlElement *xml) {
  if (xml != nullptr) {
    xml->setAttribute("pSteps", pSteps);
    xml->setAttribute("pBeats", pBeats);
    xml->setAttribute("pOffset", pOffset);
    xml->setAttribute("rSteps", rSteps);
    xml->setAttribute("rBeats", rBeats);
    xml->setAttribute("rOffset", rOffset);

    xml->setAttribute("macroPSteps", macroPSteps);
    xml->setAttribute("macroPBeats", macroPBeats);
    xml->setAttribute("macroPOffset", macroPOffset);
    xml->setAttribute("macroRSteps", macroRSteps);
    xml->setAttribute("macroRBeats", macroRBeats);
    xml->setAttribute("macroROffset", macroROffset);

    xml->setAttribute("transportSyncMode", transportSyncMode);
    xml->setAttribute("patternResetOnRelease", patternResetOnRelease ? 1 : 0);
    xml->setAttribute("rhythmResetOnRelease", rhythmResetOnRelease ? 1 : 0);
    xml->setAttribute("clockDivisionIndex", clockDivisionIndex);
    xml->setAttribute("triplet", triplet ? 1 : 0);
    xml->setAttribute("outputChannel", outputChannel);
  }
}

void MidiOutNode::loadNodeState(juce::XmlElement *xml) {
  if (xml != nullptr) {
    pSteps = xml->getIntAttribute("pSteps", 16);
    pBeats = xml->getIntAttribute("pBeats", 4);
    pOffset = xml->getIntAttribute("pOffset", 16);
    rSteps = xml->getIntAttribute("rSteps", 16);
    rBeats = xml->getIntAttribute("rBeats", 16);
    rOffset = xml->getIntAttribute("rOffset", 16);

    macroPSteps = xml->getIntAttribute("macroPSteps", -1);
    macroPBeats = xml->getIntAttribute("macroPBeats", -1);
    macroPOffset = xml->getIntAttribute("macroPOffset", -1);
    macroRSteps = xml->getIntAttribute("macroRSteps", -1);
    macroRBeats = xml->getIntAttribute("macroRBeats", -1);
    macroROffset = xml->getIntAttribute("macroROffset", -1);

    transportSyncMode = xml->getIntAttribute("transportSyncMode", 0);
    patternResetOnRelease =
        xml->getIntAttribute("patternResetOnRelease", 1) != 0;
    rhythmResetOnRelease = xml->getIntAttribute("rhythmResetOnRelease", 1) != 0;
    clockDivisionIndex = xml->getIntAttribute("clockDivisionIndex", 5);
    triplet = xml->getIntAttribute("triplet", 0) != 0;
    outputChannel = xml->getIntAttribute("outputChannel", 1);
  }
}
