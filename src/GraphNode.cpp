#include "GraphNode.h"

#include <algorithm>

void GraphNode::setInputSequence(int inputPort, const EventSequence &sequence) {
  inputSequences[inputPort] = sequence;
}

void GraphNode::clearInputSequence(int inputPort) {
  inputSequences.erase(inputPort);
}

const EventSequence &GraphNode::getInputSequence(int inputPort) const {
  auto it = inputSequences.find(inputPort);
  if (it != inputSequences.end()) {
    return it->second;
  }

  static const EventSequence emptySequence;
  return emptySequence;
}

const EventSequence &GraphNode::getOutputSequence(int outputPort) const {
  auto it = outputSequences.find(outputPort);
  if (it != outputSequences.end()) {
    return it->second;
  }

  static const EventSequence emptySequence;
  return emptySequence;
}

void GraphNode::setOutputSequence(int outputPort,
                                  const EventSequence &sequence) {
  outputSequences[outputPort] = sequence;
}

void GraphNode::addConnection(int thisOutputPort, GraphNode *targetNode,
                              int targetInputPort) {
  connections[thisOutputPort].push_back({targetNode, targetInputPort});
}

void GraphNode::removeConnectionTo(GraphNode *targetNode, int targetInputPort) {
  for (auto &[port, connVec] : connections) {
    connVec.erase(
        std::remove_if(connVec.begin(), connVec.end(),
                       [targetNode, targetInputPort](const Connection &c) {
                         return c.targetNode == targetNode &&
                                c.targetInputPort == targetInputPort;
                       }),
        connVec.end());
  }
}

void GraphNode::removeAllConnectionsTo(GraphNode *targetNode) {
  for (auto &[port, connVec] : connections) {
    connVec.erase(std::remove_if(connVec.begin(), connVec.end(),
                                 [targetNode](const Connection &c) {
                                   return c.targetNode == targetNode;
                                 }),
                  connVec.end());
  }
}

void GraphNode::clearConnections() { connections.clear(); }
