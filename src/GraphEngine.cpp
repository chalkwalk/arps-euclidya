#include "GraphEngine.h"

#include <algorithm>
#include <queue>
#include <set>
#include <unordered_map>
#include <unordered_set>

#include "MidiOutNode/MidiOutNode.h"

void GraphEngine::addNode(const std::shared_ptr<GraphNode> &node) {
  if (macrosPtr != nullptr) {
    node->macros = *macrosPtr;
  }
  if (onGraphDirtied) {
    auto callback = onGraphDirtied;
    GraphNode *rawPtr = node.get();
    node->onNodeDirtied = [rawPtr, callback]() {
      rawPtr->isDirty = true;
      callback();
    };
  }
  nodes.push_back(node);
}

std::vector<MidiOutNode *> GraphEngine::getMidiOutNodes() const {
  std::vector<MidiOutNode *> outNodes;
  for (const auto &node : nodes) {
    if (auto *out = dynamic_cast<MidiOutNode *>(node.get())) {
      outNodes.push_back(out);
    }
  }
  return outNodes;
}

void GraphEngine::removeNode(GraphNode *node) {
  // First, remove all connections involving this node
  for (auto &n : nodes) {
    if (n.get() == node) {
      // Before clearing our own connections, tell the targets to clear their
      // cached inputs from us
      for (const auto &[outPort, connVec] : n->getConnections()) {
        for (const auto &conn : connVec) {
          conn.targetNode->clearInputSequence(conn.targetInputPort);
        }
      }
      n->clearConnections();
    } else {
      // Remove connections that point to the deleted node
      n->removeAllConnectionsTo(node);
    }
  }

  // Remove the node itself
  auto it = std::remove_if(
      nodes.begin(), nodes.end(),
      [node](const std::shared_ptr<GraphNode> &n) { return n.get() == node; });
  if (it != nodes.end()) {
    nodes.erase(it, nodes.end());
  }

  recalculate();
}

bool GraphEngine::isAreaOccupied(int gridX, int gridY, int gridW, int gridH,
                                 GraphNode *ignoreNode) const {
  for (const auto &node : nodes) {
    if (node.get() == ignoreNode) {
      continue;
    }

    int curX = node->gridX;
    int curY = node->gridY;
    int curW = node->getGridWidth();
    int curH = node->getGridHeight();

    // Standard AABB intersection test (exclusive of edges)
    bool overlapX = (gridX < curX + curW) && (gridX + gridW > curX);
    bool overlapY = (gridY < curY + curH) && (gridY + gridH > curY);

    if (overlapX && overlapY) {
      return true;
    }
  }
  return false;
}

juce::Point<int> GraphEngine::findClosestFreeSpot(
    int startX, int startY, int gridW, int gridH, GraphNode *ignoreNode,
    juce::Point<int> preferredPoint) const {
  if (!isAreaOccupied(startX, startY, gridW, gridH, ignoreNode)) {
    return {startX, startY};
  }

  auto getDistSq = [](int x1, int y1, int x2, int y2) {
    long long dx = x1 - x2;
    long long dy = y1 - y2;
    return dx * dx + dy * dy;
  };

  // Expanding square ring search
  for (int radius = 1; radius < 50; ++radius) {
    std::vector<juce::Point<int>> candidates;

    // Helper to check and add candidate
    auto addIfFree = [&](int x, int y) {
      if (!isAreaOccupied(x, y, gridW, gridH, ignoreNode)) {
        candidates.push_back({x, y});
      }
    };

    // Top and Bottom rows
    for (int x = startX - radius; x <= startX + radius; ++x) {
      addIfFree(x, startY - radius);
      addIfFree(x, startY + radius);
    }
    // Left and Right columns (excluding corners)
    for (int y = startY - radius + 1; y < startY + radius; ++y) {
      addIfFree(startX - radius, y);
      addIfFree(startX + radius, y);
    }

    if (!candidates.empty()) {
      // Sort candidates by distance to target point, then by distance to
      // preferred point
      std::sort(candidates.begin(), candidates.end(),
                [&](const juce::Point<int> &a, const juce::Point<int> &b) {
                  auto dTargetA = getDistSq(a.x, a.y, startX, startY);
                  auto dTargetB = getDistSq(b.x, b.y, startX, startY);

                  if (dTargetA != dTargetB) {
                    return dTargetA < dTargetB;
                  }

                  if (preferredPoint.x != -1) {
                    auto dPrefA =
                        getDistSq(a.x, a.y, preferredPoint.x, preferredPoint.y);
                    auto dPrefB =
                        getDistSq(b.x, b.y, preferredPoint.x, preferredPoint.y);
                    return dPrefA < dPrefB;
                  }

                  return false;
                });

      return candidates[0];
    }
  }

  return {startX, startY};
}

int GraphEngine::getNextFreeMacro() const {
  std::set<int> usedMacros;
  for (const auto &node : nodes) {
    auto mappings = node->getMacroMappings();
    for (const auto &mapping : mappings) {
      if (mapping.second != nullptr && *mapping.second != -1) {
        usedMacros.insert(*mapping.second);
      }
    }
  }

  for (int i = 0; i < 32; ++i) {
    if (usedMacros.find(i) == usedMacros.end()) {
      return i;
    }
  }

  return -1;
}

bool GraphEngine::addExplicitConnection(GraphNode *source, int outPort,

                                        GraphNode *target, int inPort) {
  if (source == nullptr || target == nullptr) {
    return false;
  }
  if (source == target) {
    return false;
  }

  // Cycle detection: would adding source→target create a cycle?
  if (wouldCreateCycle(source, target)) {
    return false;
  }

  // Check port bounds
  if (outPort < 0 || outPort >= source->getNumOutputPorts()) {
    return false;
  }
  if (inPort < 0 || inPort >= target->getNumInputPorts()) {
    return false;
  }

  // Remove any existing connection to this specific input port
  // (an input port can only have one source)
  for (auto &n : nodes) {
    n->removeConnectionTo(target, inPort);
  }

  source->addConnection(outPort, target, inPort);
  if (onTopologyChanged) {
    onTopologyChanged();
  }
  recalculate();
  return true;
}

void GraphEngine::removeConnection(GraphNode *source, int outPort,
                                   GraphNode *target, int inPort) {
  if (source == nullptr) {
    return;
  }

  auto it = source->getConnections().find(outPort);
  if (it != source->getConnections().end()) {
    auto sizeBefore = it->second.size();
    source->removeConnectionTo(target, inPort);

    // We need to re-find it because removeConnectionTo doesn't return size
    auto itAfter = source->getConnections().find(outPort);
    if (itAfter != source->getConnections().end() &&
        itAfter->second.size() < sizeBefore && target != nullptr) {
      target->clearInputSequence(inPort);
      target->isDirty = true;
      if (onTopologyChanged) {
        onTopologyChanged();
      }
      recalculate();
    }
  }
}

bool GraphEngine::isInputPortOccupied(GraphNode *targetNode, int inPort) const {
  for (const auto &n : nodes) {
    for (const auto &[port, connVec] : n->getConnections()) {
      for (const auto &conn : connVec) {
        if (conn.targetNode == targetNode && conn.targetInputPort == inPort) {
          return true;
        }
      }
    }
  }
  return false;
}

bool GraphEngine::isOutputPortOccupied(GraphNode *node, int portIndex) {
  const auto &conns = node->getConnections();
  auto it = conns.find(portIndex);
  if (it != conns.end()) {
    return !it->second.empty();
  }
  return false;
}

bool GraphEngine::wouldCreateCycle(GraphNode *source, GraphNode *target) const {
  // If target can reach source via existing connections, adding source→target
  // would create a cycle
  return isReachable(target, source);
}

bool GraphEngine::isReachable(GraphNode *from, GraphNode *to) {
  if (from == to) {
    return true;
  }

  std::unordered_set<GraphNode *> visited;
  std::queue<GraphNode *> queue;
  queue.push(from);
  visited.insert(from);

  while (!queue.empty()) {
    GraphNode *current = queue.front();
    queue.pop();

    for (const auto &[port, connVec] : current->getConnections()) {
      for (const auto &conn : connVec) {
        if (conn.targetNode == to) {
          return true;
        }
        if (visited.find(conn.targetNode) == visited.end()) {
          visited.insert(conn.targetNode);
          queue.push(conn.targetNode);
        }
      }
    }
  }

  return false;
}

std::vector<GraphNode *> GraphEngine::topologicalSort() const {
  // Kahn's algorithm
  std::unordered_map<GraphNode *, int> inDegree;
  for (const auto &node : nodes) {
    if (inDegree.find(node.get()) == inDegree.end()) {
      inDegree[node.get()] = 0;
    }
  }

  // Count incoming edges
  for (const auto &node : nodes) {
    for (const auto &[port, connVec] : node->getConnections()) {
      for (const auto &conn : connVec) {
        inDegree[conn.targetNode]++;
      }
    }
  }

  // Start with nodes that have no incoming edges
  std::queue<GraphNode *> queue;
  for (const auto &node : nodes) {
    if (inDegree[node.get()] == 0) {
      queue.push(node.get());
    }
  }

  std::vector<GraphNode *> sorted;
  while (!queue.empty()) {
    GraphNode *current = queue.front();
    queue.pop();
    sorted.push_back(current);

    for (const auto &[port, connVec] : current->getConnections()) {
      for (const auto &conn : connVec) {
        inDegree[conn.targetNode]--;
        if (inDegree[conn.targetNode] == 0) {
          queue.push(conn.targetNode);
        }
      }
    }
  }

  return sorted;
}

void GraphEngine::recalculate() {
  auto sorted = topologicalSort();

  for (GraphNode *node : sorted) {
    if (!node->isDirty)
      continue;
    node->isDirty = false;

    if (node->bypassed) {
      if (node->getNumInputPorts() > 0 && node->getNumOutputPorts() > 0) {
        node->setOutputSequence(0, node->getInputSequence(0));
      }
    } else {
      node->process();
    }

    // 2. Propagate its outputs forward to targets
    for (const auto &[outPort, connVec] : node->getConnections()) {
      const auto &outSeq = node->getOutputSequence(outPort);
      for (const auto &conn : connVec) {
        conn.targetNode->setInputSequence(conn.targetInputPort, outSeq);
        conn.targetNode->isDirty = true;
      }
    }
  }
}

void GraphEngine::saveState(juce::XmlElement *xmlRoot) {
  if (xmlRoot == nullptr) {
    return;
  }

  auto *nodesXml = xmlRoot->createNewChildElement("Nodes");
  for (const auto &node : nodes) {
    auto *nodeXml = nodesXml->createNewChildElement("Node");
    nodeXml->setAttribute("type", node->getName());
    nodeXml->setAttribute("uuid", node->nodeId.toString());
    node->saveBaseState(nodeXml);
    node->saveNodeState(nodeXml);
  }

  auto *connectionsXml = xmlRoot->createNewChildElement("Connections");
  for (const auto &node : nodes) {
    for (const auto &connList : node->getConnections()) {
      int outPort = connList.first;
      for (const auto &conn : connList.second) {
        auto *connXml = connectionsXml->createNewChildElement("Connection");
        connXml->setAttribute("sourceUuid", node->nodeId.toString());
        connXml->setAttribute("sourcePort", outPort);
        connXml->setAttribute("targetUuid", conn.targetNode->nodeId.toString());
        connXml->setAttribute("targetPort", conn.targetInputPort);
      }
    }
  }
}

#include "NodeFactory.h"

void GraphEngine::loadState(juce::XmlElement *xmlRoot,
                            NoteExpressionManager &midiCtx,
                            ClockManager &clockCtx,
                            std::array<std::atomic<float> *, 32> &macros) {
  if (xmlRoot == nullptr) {
    return;
  }

  nodes.clear();

  // Load Nodes
  auto *nodesXml = xmlRoot->getChildByName("Nodes");
  if (nodesXml != nullptr) {
    for (auto *nodeXml : nodesXml->getChildIterator()) {
      std::string type = nodeXml->getStringAttribute("type").toStdString();
      auto node = NodeFactory::createNode(type, midiCtx, clockCtx);
      if (node) {
        node->nodeId = juce::Uuid(nodeXml->getStringAttribute("uuid"));
        node->loadBaseState(nodeXml);
        node->loadNodeState(nodeXml);
        nodes.push_back(node);
      }
    }
  }

  // Load Connections
  auto *connectionsXml = xmlRoot->getChildByName("Connections");
  if (connectionsXml != nullptr) {
    for (auto *connXml : connectionsXml->getChildIterator()) {
      juce::String sourceUuidStr = connXml->getStringAttribute("sourceUuid");
      int sourcePort = connXml->getIntAttribute("sourcePort");
      juce::String targetUuidStr = connXml->getStringAttribute("targetUuid");
      int targetPort = connXml->getIntAttribute("targetPort");

      GraphNode *sourceNode = nullptr;
      GraphNode *targetNode = nullptr;

      for (const auto &node : nodes) {
        if (node->nodeId.toString() == sourceUuidStr) {
          sourceNode = node.get();
        }
        if (node->nodeId.toString() == targetUuidStr) {
          targetNode = node.get();
        }
      }

      if (sourceNode && targetNode) {
        sourceNode->addConnection(sourcePort, targetNode, targetPort);
      }
    }
  }

  // Wire callbacks, set macros, and mark dirty
  for (const auto &node : nodes) {
    node->macros = macros;
    if (onGraphDirtied) {
      auto callback = onGraphDirtied;
      GraphNode *rawPtr = node.get();
      node->onNodeDirtied = [rawPtr, callback]() {
        rawPtr->isDirty = true;
        callback();
      };
    }
    node->isDirty = true;
  }
  recalculate();
}
