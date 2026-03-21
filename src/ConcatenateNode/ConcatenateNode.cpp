#include "ConcatenateNode.h"

void ConcatenateNode::process() {
  NoteSequence result;

  auto it0 = inputSequences.find(0);
  if (it0 != inputSequences.end()) {
    result.insert(result.end(), it0->second.begin(), it0->second.end());
  }

  auto it1 = inputSequences.find(1);
  if (it1 != inputSequences.end()) {
    result.insert(result.end(), it1->second.begin(), it1->second.end());
  }

  outputSequences[0] = result;
}
