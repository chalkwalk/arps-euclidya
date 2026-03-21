#include "ReverseNode.h"
#include <algorithm>

void ReverseNode::process() {
  auto it = inputSequences.find(0);
  if (it == inputSequences.end() || it->second.empty()) {
    outputSequences[0] = NoteSequence();
  } else {
    // 1. Copy the sequence
    NoteSequence reversedSeq = it->second;

    // 2. Reverse the step order
    std::reverse(reversedSeq.begin(), reversedSeq.end());

    // 3. Cache to output
    outputSequences[0] = reversedSeq;
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
