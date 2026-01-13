#include "MidiOutNode.h"

MidiOutNode::MidiOutNode(MidiHandler &midiCtx, ClockManager &clockCtx,
                         std::array<std::atomic<float> *, 32> macrosArray)
    : midiHandler(midiCtx), clockManager(clockCtx), macros(macrosArray) {}

void MidiOutNode::process() {
  // Graph changed upstream, reset sequence logic if the length shrunk
  auto it = inputSequences.find(0);
  if (it != inputSequences.end()) {
    int numSteps = it->second.size();
    if (numSteps > 0 && sequenceIndex >= numSteps) {
      sequenceIndex = 0;
    } else if (numSteps == 0) {
      sequenceIndex = 0;
    }
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
    int rSteps = macros[3] != nullptr ? (int)std::round(macros[3]->load()) : 16;
    int rBeats = macros[4] != nullptr ? (int)std::round(macros[4]->load()) : 16;
    int rOffset =
        macros[5] != nullptr ? (int)std::round(macros[5]->load()) : 16;

    std::vector<bool> rhythmPattern =
        EuclideanMath::generatePattern(rSteps, rBeats, rOffset);

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
    int pSteps = macros[0] != nullptr ? (int)std::round(macros[0]->load()) : 16;
    int pBeats = macros[1] != nullptr ? (int)std::round(macros[1]->load()) : 4;
    int pOffset =
        macros[2] != nullptr ? (int)std::round(macros[2]->load()) : 16;

    std::vector<bool> pattern =
        EuclideanMath::generatePattern(pSteps, pBeats, pOffset);

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
