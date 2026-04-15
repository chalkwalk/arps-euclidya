#include "ZipNode.h"

#include <algorithm>

void ZipNode::process() {
  auto it0 = inputSequences.find(0);
  auto it1 = inputSequences.find(1);

  NoteSequence seq0 =
      (it0 != inputSequences.end()) ? it0->second : NoteSequence{};
  NoteSequence seq1 =
      (it1 != inputSequences.end()) ? it1->second : NoteSequence{};

  size_t maxLen = std::max(seq0.size(), seq1.size());
  NoteSequence outSeq;
  outSeq.reserve(maxLen);

  for (size_t i = 0; i < maxLen; ++i) {
    EventStep step;
    if (i < seq0.size()) {
      for (const auto &ev : seq0[i]) step.push_back(ev);
    }
    if (i < seq1.size()) {
      for (const auto &ev : seq1[i]) step.push_back(ev);
    }

    // Merge notes with the same pitch. When two notes' MpeConditions can be
    // hulled (all axes touch or overlap), replace both with the single hulled
    // note. When any axis is truly disjoint, keep both — they will
    // independently fire or rest at playback time based on their conditions.
    // CC events pass through without merging.
    EventStep merged;
    merged.reserve(step.size());
    for (const auto &ev : step) {
      const auto *note = asNote(ev);
      if (!note) {
        merged.push_back(ev);  // CC events: pass through
        continue;
      }
      bool absorbed = false;
      for (auto &m : merged) {
        auto *mn = asNote(m);
        if (mn && mn->noteNumber == note->noteNumber) {
          MpeCondition hulled;
          if (MpeCondition::tryHull(mn->mpeCondition, note->mpeCondition,
                                    hulled)) {
            mn->mpeCondition = hulled;
            absorbed = true;
            break;
          }
        }
      }
      if (!absorbed) {
        merged.push_back(ev);
      }
    }

    std::sort(merged.begin(), merged.end(),
              [](const SequenceEvent &a, const SequenceEvent &b) {
                const auto *na = asNote(a);
                const auto *nb = asNote(b);
                return (na ? na->noteNumber : -1) < (nb ? nb->noteNumber : -1);
              });
    outSeq.push_back(merged);
  }

  outputSequences[0] = outSeq;

  auto connIt = connections.find(0);
  if (connIt != connections.end()) {
    for (const auto &connection : connIt->second) {
      connection.targetNode->setInputSequence(connection.targetInputPort,
                                              outputSequences[0]);
    }
  }
}
