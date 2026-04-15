#include "ConvergeNode.h"

#include <algorithm>

void ConvergeNode::process() {
  auto it = inputSequences.find(0);
  if (it == inputSequences.end() || it->second.empty()) {
    outputSequences[0] = NoteSequence();
  } else {
    NoteSequence steps = it->second;

    auto getMeanValue = [](const EventStep &step) {
      float sum = 0.0f;
      int count = 0;
      for (const auto &ev : step) {
        if (const auto *n = asNote(ev)) {
          sum += (float)n->noteNumber;
          ++count;
        }
      }
      return count > 0 ? sum / (float)count : 0.0f;
    };

    std::stable_sort(steps.begin(), steps.end(),
                     [&getMeanValue](const EventStep &a, const EventStep &b) {
                       return getMeanValue(a) < getMeanValue(b);
                     });

    NoteSequence sortedSeq;
    int s = 0;
    int e = (int)steps.size() - 1;
    while (s <= e) {
      sortedSeq.push_back(steps[(size_t)s]);  // preserve rests
      if (s != e) {
        sortedSeq.push_back(steps[(size_t)e]);  // preserve rests
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
