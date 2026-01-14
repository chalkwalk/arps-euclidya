#include "SortNode.h"
#include <algorithm>

void SortNode::process() {
  auto it = inputSequences.find(0);
  if (it == inputSequences.end() || it->second.empty()) {
    outputSequences[0] = NoteSequence();
  } else {
    // 1. Extract all currently held notes from the incoming sequence into a
    // flat list
    std::vector<HeldNote> flatNotes;
    for (const auto &step : it->second) {
      for (const auto &note : step) {
        flatNotes.push_back(note);
      }
    }

    // 2. Sort the notes ascending by pitch
    std::sort(flatNotes.begin(), flatNotes.end(),
              [](const HeldNote &a, const HeldNote &b) {
                return a.noteNumber < b.noteNumber;
              });

    // 3. Reconstruct into a new sorted NoteSequence (1 note per step)
    NoteSequence sortedSeq;
    for (const auto &note : flatNotes) {
      sortedSeq.push_back({note});
    }

    outputSequences[0] = sortedSeq;
  }

  // 4. Pass down the graph
  auto connIt = connections.find(0);
  if (connIt != connections.end()) {
    for (const auto &connection : connIt->second) {
      connection.targetNode->setInputSequence(connection.targetInputPort,
                                              outputSequences[0]);
    }
  }
}
