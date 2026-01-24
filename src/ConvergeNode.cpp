#include "ConvergeNode.h"
#include <algorithm>

void ConvergeNode::process() {
  auto it = inputSequences.find(0);
  if (it == inputSequences.end() || it->second.empty()) {
    outputSequences[0] = NoteSequence();
  } else {
    std::vector<HeldNote> flatNotes;
    for (const auto &step : it->second) {
      for (const auto &note : step) {
        flatNotes.push_back(note);
      }
    }

    std::sort(flatNotes.begin(), flatNotes.end(),
              [](const HeldNote &a, const HeldNote &b) {
                return a.noteNumber < b.noteNumber;
              });

    NoteSequence sortedSeq;
    int s = 0;
    int e = (int)flatNotes.size() - 1;
    while (s <= e) {
      sortedSeq.push_back({flatNotes[s]});
      if (s != e) {
        sortedSeq.push_back({flatNotes[e]});
      }
      s++;
      e--;
    }

    outputSequences[0] = sortedSeq;
  }

  auto connIt = connections.find(0);
  if (connIt != connections.end()) {
    for (const auto &connection : connIt->second) {
      connection.targetNode->setInputSequence(connection.targetInputPort,
                                              outputSequences[0]);
    }
  }
}
