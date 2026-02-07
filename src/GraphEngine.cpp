#include "GraphEngine.h"
#include <algorithm>
#include <queue>
#include <set>
#include <unordered_map>
#include <unordered_set>

void GraphEngine::addNode(std::shared_ptr<GraphNode> node) {
  nodes.push_back(node);
}

void GraphEngine::removeNode(GraphNode *node) {
  // First, remove all connections involving this node
  for (auto &n : nodes) {
    if (n.get() == node) {
      n->clearConnections();
    } else {
      // Remove connections that point to the deleted node
      auto &conns =
          const_cast<std::map<int, std::vector<GraphNode::Connection>> &>(
              n->getConnections());
      for (auto &[port, connVec] : conns) {
        connVec.erase(std::remove_if(connVec.begin(), connVec.end(),
                                     [node](const GraphNode::Connection &c) {
                                       return c.targetNode == node;
                                     }),
                      connVec.end());
      }
    }
  }

  // Remove the node itself
  auto it = std::remove_if(
      nodes.begin(), nodes.end(),
      [node](const std::shared_ptr<GraphNode> &n) { return n.get() == node; });
  if (it != nodes.end()) {
    nodes.erase(it, nodes.end());
  }
}

bool GraphEngine::addExplicitConnection(GraphNode *source, int outPort,
                                        GraphNode *target, int inPort) {
  if (source == nullptr || target == nullptr)
    return false;
  if (source == target)
    return false;

  // Cycle detection: would adding source→target create a cycle?
  if (wouldCreateCycle(source, target))
    return false;

  // Check port bounds
  if (outPort < 0 || outPort >= source->getNumOutputPorts())
    return false;
  if (inPort < 0 || inPort >= target->getNumInputPorts())
    return false;

  // Remove any existing connection to this specific input port
  // (an input port can only have one source)
  for (auto &n : nodes) {
    auto &conns =
        const_cast<std::map<int, std::vector<GraphNode::Connection>> &>(
            n->getConnections());
    for (auto &[port, connVec] : conns) {
      connVec.erase(
          std::remove_if(connVec.begin(), connVec.end(),
                         [target, inPort](const GraphNode::Connection &c) {
                           return c.targetNode == target &&
                                  c.targetInputPort == inPort;
                         }),
          connVec.end());
    }
  }

  source->addConnection(outPort, target, inPort);
  return true;
}

void GraphEngine::removeConnection(GraphNode *source, int outPort,
                                   GraphNode *target, int inPort) {
  if (source == nullptr)
    return;

  auto &conns = const_cast<std::map<int, std::vector<GraphNode::Connection>> &>(
      source->getConnections());
  auto it = conns.find(outPort);
  if (it != conns.end()) {
    auto sizeBefore = it->second.size();
    it->second.erase(
        std::remove_if(it->second.begin(), it->second.end(),
                       [target, inPort](const GraphNode::Connection &c) {
                         return c.targetNode == target &&
                                c.targetInputPort == inPort;
                       }),
        it->second.end());

    // If we actually removed a connection, clear the target's cached input
    if (it->second.size() < sizeBefore && target != nullptr) {
      target->clearInputSequence(inPort);
    }
  }
}

bool GraphEngine::wouldCreateCycle(GraphNode *source, GraphNode *target) const {
  // If target can reach source via existing connections, adding source→target
  // would create a cycle
  return isReachable(target, source);
}

bool GraphEngine::isReachable(GraphNode *from, GraphNode *to) const {
  if (from == to)
    return true;

  std::unordered_set<GraphNode *> visited;
  std::queue<GraphNode *> queue;
  queue.push(from);
  visited.insert(from);

  while (!queue.empty()) {
    GraphNode *current = queue.front();
    queue.pop();

    for (const auto &[port, connVec] : current->getConnections()) {
      for (const auto &conn : connVec) {
        if (conn.targetNode == to)
          return true;
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
    // Copy outputs of upstream nodes to this node's inputs
    for (const auto &upstreamNodePtr : nodes) {
      for (const auto &[outPort, connVec] : upstreamNodePtr->getConnections()) {
        for (const auto &conn : connVec) {
          if (conn.targetNode == node) {
            node->setInputSequence(conn.targetInputPort,
                                   upstreamNodePtr->getOutputSequence(outPort));
          }
        }
      }
    }
    node->process();
  }
}

void GraphEngine::saveState(juce::XmlElement *xmlRoot) {
  if (xmlRoot == nullptr)
    return;

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

void GraphEngine::loadState(juce::XmlElement *xmlRoot, MidiHandler &midiCtx,
                            ClockManager &clockCtx,
                            std::array<std::atomic<float> *, 32> &macros) {
  if (xmlRoot == nullptr)
    return;

  nodes.clear();

  // Load Nodes
  auto *nodesXml = xmlRoot->getChildByName("Nodes");
  if (nodesXml != nullptr) {
    for (auto *nodeXml : nodesXml->getChildIterator()) {
      std::string type = nodeXml->getStringAttribute("type").toStdString();
      auto node = NodeFactory::createNode(type, midiCtx, clockCtx, macros);
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
        if (node->nodeId.toString() == sourceUuidStr)
          sourceNode = node.get();
        if (node->nodeId.toString() == targetUuidStr)
          targetNode = node.get();
      }

      if (sourceNode && targetNode) {
        sourceNode->addConnection(sourcePort, targetNode, targetPort);
      }
    }
  }
}
