#include "UnzipNode.h"

#include <algorithm>

void UnzipNode::process() {
  auto it = inputSequences.find(0);
  if (it == inputSequences.end() || it->second.empty()) {
    outputSequences[0] = {};
    outputSequences[1] = {};
    return;
  }

  NoteSequence out0;
  NoteSequence out1;
  const auto &seq = it->second;

  for (const auto &step : seq) {
    if (step.empty()) {
      out0.emplace_back();
      out1.emplace_back();
      continue;
    }

    auto sortedStep = step;
    std::sort(sortedStep.begin(), sortedStep.end(),
              [](const SequenceEvent &a, const SequenceEvent &b) {
                const auto *na = asNote(a);
                const auto *nb = asNote(b);
                return (na ? na->noteNumber : 0) < (nb ? nb->noteNumber : 0);
              });

    size_t half = sortedStep.size() / 2;
    if (sortedStep.size() == 1) {
      out0.push_back(sortedStep);
      out1.emplace_back();
    } else {
      EventStep lower(sortedStep.begin(),
                      sortedStep.begin() + static_cast<ptrdiff_t>(half));
      EventStep higher(sortedStep.begin() + static_cast<ptrdiff_t>(half),
                       sortedStep.end());
      out0.push_back(lower);
      out1.push_back(higher);
    }
  }

  outputSequences[0] = out0;
  outputSequences[1] = out1;

  auto connIt0 = connections.find(0);
  if (connIt0 != connections.end()) {
    for (const auto &connection : connIt0->second) {
      connection.targetNode->setInputSequence(connection.targetInputPort,
                                              outputSequences[0]);
    }
  }

  auto connIt1 = connections.find(1);
  if (connIt1 != connections.end()) {
    for (const auto &connection : connIt1->second) {
      connection.targetNode->setInputSequence(connection.targetInputPort,
                                              outputSequences[1]);
    }
  }
}
