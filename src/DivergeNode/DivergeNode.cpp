#include "DivergeNode.h"

#include <algorithm>

void DivergeNode::process() {
  auto it = inputSequences.find(0);
  if (it == inputSequences.end() || it->second.empty()) {
    outputSequences[0] = NoteSequence();
  } else {
    NoteSequence steps = it->second;

    auto getMeanValue = [](const std::vector<HeldNote> &chord) {
      if (chord.empty()) {
        return 0.0f;
      }
      float sum = 0.0f;
      for (const auto &n : chord) {
        sum += n.noteNumber;
      }
      return sum / (float)chord.size();
    };

    std::stable_sort(steps.begin(), steps.end(),
                     [&getMeanValue](const std::vector<HeldNote> &a,
                                     const std::vector<HeldNote> &b) {
                       return getMeanValue(a) < getMeanValue(b);
                     });

    NoteSequence sortedSeq;
    int s = ((int)steps.size() - 1) / 2;
    int e = (int)steps.size() / 2;

    while (s >= 0 || e < (int)steps.size()) {
      if (s >= 0) {
        sortedSeq.push_back(steps[(size_t)s]);  // preserve rests
      }
      if (s != e && e < (int)steps.size()) {
        sortedSeq.push_back(steps[(size_t)e]);  // preserve rests
      }
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
