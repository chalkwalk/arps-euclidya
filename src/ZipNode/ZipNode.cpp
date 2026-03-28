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
    std::vector<HeldNote> step;
    if (i < seq0.size()) {
      for (const auto &n : seq0[i]) {
        step.push_back(n);
      }
    }
    if (i < seq1.size()) {
      for (const auto &n : seq1[i]) {
        step.push_back(n);
      }
    }

    std::sort(step.begin(), step.end(),
              [](const HeldNote &a, const HeldNote &b) {
                return a.noteNumber < b.noteNumber;
              });
    auto last = std::unique(step.begin(), step.end(),
                            [](const HeldNote &a, const HeldNote &b) {
                              return a.noteNumber == b.noteNumber;
                            });
    step.erase(last, step.end());

    outSeq.push_back(step);
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
