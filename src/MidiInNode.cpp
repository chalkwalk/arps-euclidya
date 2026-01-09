#include "MidiInNode.h"

MidiInNode::MidiInNode(MidiHandler &handler) : midiHandler(handler) {}

void MidiInNode::process() {
  auto heldNotes = midiHandler.getHeldNotes();

  NoteSequence seq;

  // Convert 1D list of HeldNotes into 2D NoteSequence where each step is one
  // note
  for (const auto &note : heldNotes) {
    std::vector<HeldNote> step = {note};
    seq.push_back(step);
  }

  // Cache the sequence at output port 0
  outputSequences[0] = seq;

  // Push the sequence downstream to all connected nodes
  auto it = connections.find(0);
  if (it != connections.end()) {
    for (const auto &connection : it->second) {
      connection.targetNode->setInputSequence(connection.targetInputPort, seq);
    }
  }
}
