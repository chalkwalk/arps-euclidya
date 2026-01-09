#include "MidiOutNode.h"

MidiOutNode::MidiOutNode(MidiHandler &midiCtx, ClockManager &clockCtx)
    : midiHandler(midiCtx), clockManager(clockCtx) {}

void MidiOutNode::process() {
  // The graph changed upstream, we need to reset the index if the length shrunk
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
  // Release any notes currently held from the previous step BEFORE looking for
  // ticks In a real ARP, Note Offs are triggered by a Gate Length / PPQN
  // sub-tick For Step 2, we just kill the previous note exactly when the new
  // note triggers
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
    const auto &step = sequence[sequenceIndex];

    // Ensure index wraps properly
    sequenceIndex = (sequenceIndex + 1) % sequence.size();

    for (const HeldNote &noteTrigger : step) {
      // --- DYNAMIC MPE POLLING ---
      // Fetch the **current** physical state from the controller for this note
      // even though the graph might have seen the trigger seconds ago
      float currentPressure = midiHandler.getMpeZ(noteTrigger.noteNumber);

      // For Step 2, let's map pressure (Z) to Velocity to prove out the
      // decouple Base velocity + modulated pressure
      float finalVelocity = std::clamp(
          noteTrigger.velocity + (currentPressure * 0.5f), 0.0f, 1.0f);

      outputBuffer.addEvent(juce::MidiMessage::noteOn(noteTrigger.channel,
                                                      noteTrigger.noteNumber,
                                                      finalVelocity),
                            samplePosition);

      // Keep track so we can Note Off on the next beat
      playingNotes.push_back({noteTrigger.channel, noteTrigger.noteNumber});

      // Note: Continuous messages (Pitch Bend, CC74) for actively playing notes
      // should realistically be pushed out every audio block, not just on
      // ticks. But this proves the decoupled Note-On modulation concept.
    }
  }
}
