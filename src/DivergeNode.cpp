#include "DivergeNode.h"
#include <algorithm>

void DivergeNode::process() {
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
    int s = ((int)flatNotes.size() - 1) / 2;
    int e = (int)flatNotes.size() / 2;

    while (s >= 0 || e < flatNotes.size()) {
      if (s >= 0)
        sortedSeq.push_back({flatNotes[s]});
      if (s != e && e < flatNotes.size())
        sortedSeq.push_back({flatNotes[e]});
      s--;
      e++;
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
