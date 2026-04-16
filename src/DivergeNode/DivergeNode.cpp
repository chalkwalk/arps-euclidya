#include "DivergeNode.h"

#include <algorithm>

void DivergeNode::process() {
  auto it = inputSequences.find(0);
  if (it == inputSequences.end() || it->second.empty()) {
    outputSequences[0] = NoteSequence();
  } else {
    NoteSequence steps = it->second;

    // Sort key: velocity-weighted mean pitch for note steps (so loud notes pull
    // the centre toward their pitch), or mean CC value (0-127) for CC steps.
    auto getMeanValue = [](const EventStep &step) {
      float weightedPitchSum = 0.0f;
      float velocitySum = 0.0f;
      float ccSum = 0.0f;
      int ccCount = 0;
      for (const auto &ev : step) {
        if (const auto *n = asNote(ev)) {
          float vel = (float)n->velocity;
          weightedPitchSum += (float)n->noteNumber * vel;
          velocitySum += vel;
        } else if (const auto *c = asCC(ev)) {
          ccSum += c->value * 127.0f;
          ++ccCount;
        }
      }
      if (velocitySum > 0.0f)
        return weightedPitchSum / velocitySum;
      if (ccCount > 0)
        return ccSum / (float)ccCount;
      return 0.0f;
    };

    std::stable_sort(steps.begin(), steps.end(),
                     [&getMeanValue](const EventStep &a, const EventStep &b) {
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
