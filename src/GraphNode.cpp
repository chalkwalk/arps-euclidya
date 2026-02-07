#include "GraphNode.h"

void GraphNode::setInputSequence(int inputPort, const NoteSequence &sequence) {
  inputSequences[inputPort] = sequence;
}

void GraphNode::clearInputSequence(int inputPort) {
  inputSequences.erase(inputPort);
}

const NoteSequence &GraphNode::getOutputSequence(int outputPort) const {
  auto it = outputSequences.find(outputPort);
  if (it != outputSequences.end())
    return it->second;

  static const NoteSequence emptySequence;
  return emptySequence;
}

void GraphNode::addConnection(int thisOutputPort, GraphNode *targetNode,
                              int targetInputPort) {
  connections[thisOutputPort].push_back({targetNode, targetInputPort});
}

void GraphNode::clearConnections() { connections.clear(); }
