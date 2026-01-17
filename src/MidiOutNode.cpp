#include "MidiOutNode.h"

MidiOutNode::MidiOutNode(MidiHandler &midiCtx, ClockManager &clockCtx,
                         std::array<std::atomic<float> *, 32> macrosArray)
    : midiHandler(midiCtx), clockManager(clockCtx), macros(macrosArray) {}

namespace {
bool stepsAreEqual(const std::vector<HeldNote> &a,
                   const std::vector<HeldNote> &b) {
  if (a.size() != b.size())
    return false;
  // Assumes notes within a step are sorted or at least in the same order
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

  // Spiral search forward
  for (int i = 0; i < nL; ++i) {
    int cI = (previousIndex + i) % nL;
    if (stepsAreEqual(newSequence[cI], stepToFind))
      return cI;
  }

  // Fallback search from beginning
  for (int i = 0; i < nL; ++i) {
    if (stepsAreEqual(newSequence[i], stepToFind))
      return i;
  }

  // Proportional index fallback
  int oL = std::max(1, (int)oldSequence.size());
  return ((previousIndex * nL) / oL) % nL;
}
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
  // Kill prev notes unconditionally on the tick for now
  bool isTick = clockManager.isTick();

  if (isTick) {
    for (const auto &note : playingNotes) {
      outputBuffer.addEvent(juce::MidiMessage::noteOff(note.first, note.second),
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
    int actualRSteps = macroRSteps != -1 && macros[macroRSteps] != nullptr
                           ? (int)std::round(macros[macroRSteps]->load())
                           : rSteps;
    int actualRBeats = macroRBeats != -1 && macros[macroRBeats] != nullptr
                           ? (int)std::round(macros[macroRBeats]->load())
                           : rBeats;
    int actualROffset = macroROffset != -1 && macros[macroROffset] != nullptr
                            ? (int)std::round(macros[macroROffset]->load())
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

    // If it's a Rest, time passes but absolutely nothing triggers
    if (!isRhythmBeat) {
      return;
    }

    // --- 2. PATTERN LAYER (Note vs Skip) ---
    // We only evaluate this IF the Rhythm layer produced a Beat trigger
    int actualPSteps = macroPSteps != -1 && macros[macroPSteps] != nullptr
                           ? (int)std::round(macros[macroPSteps]->load())
                           : pSteps;
    int actualPBeats = macroPBeats != -1 && macros[macroPBeats] != nullptr
                           ? (int)std::round(macros[macroPBeats]->load())
                           : pBeats;
    int actualPOffset = macroPOffset != -1 && macros[macroPOffset] != nullptr
                            ? (int)std::round(macros[macroPOffset]->load())
                            : pOffset;

    std::vector<bool> pattern = EuclideanMath::generatePattern(
        actualPSteps, actualPBeats, actualPOffset);

    if (pattern.empty())
      return;

    // Safety check - we must wrap if pattern length dynamically shrunk
    if (patternIndex >= (int)pattern.size()) {
      patternIndex = 0;
    }

    // Instantaneously skip sequence steps until the pattern dictates a Beat
    size_t skipsProcessed = 0; // limit to prevent infinite loops (e.g. 0 beats)
    while (!pattern[patternIndex] && skipsProcessed < pattern.size()) {
      patternIndex = (int)((patternIndex + 1) % pattern.size());
      sequenceIndex = (int)((sequenceIndex + 1) % sequence.size());
      skipsProcessed++;
    }

    // If the loop broke normally, we have a Note to play
    if (pattern[patternIndex]) {
      const auto &step = sequence[sequenceIndex];

      // Increment for NEXT clock tick
      patternIndex = (int)((patternIndex + 1) % pattern.size());
      sequenceIndex = (int)((sequenceIndex + 1) % sequence.size());

      for (const HeldNote &noteTrigger : step) {
        // --- DYNAMIC MPE POLLING ---
        float currentPressure = midiHandler.getMpeZ(noteTrigger.noteNumber);
        float finalVelocity = std::clamp(
            noteTrigger.velocity + (currentPressure * 0.5f), 0.0f, 1.0f);

        outputBuffer.addEvent(juce::MidiMessage::noteOn(noteTrigger.channel,
                                                        noteTrigger.noteNumber,
                                                        finalVelocity),
                              samplePosition);

        playingNotes.push_back({noteTrigger.channel, noteTrigger.noteNumber});
      }
    }
  }
}
