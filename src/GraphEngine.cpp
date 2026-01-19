#include "GraphEngine.h"
#include <algorithm>

void GraphEngine::addNode(std::shared_ptr<GraphNode> node) {
  nodes.push_back(node);
  updateImplicitConnections();
}

void GraphEngine::removeNode(GraphNode *node) {
  auto it = std::remove_if(
      nodes.begin(), nodes.end(),
      [node](const std::shared_ptr<GraphNode> &n) { return n.get() == node; });
  if (it != nodes.end()) {
    nodes.erase(it, nodes.end());
    updateImplicitConnections();
  }
}

void GraphEngine::moveNode(GraphNode *node, int newIndex) {
  auto it = std::find_if(
      nodes.begin(), nodes.end(),
      [node](const std::shared_ptr<GraphNode> &n) { return n.get() == node; });

  if (it != nodes.end()) {
    std::shared_ptr<GraphNode> n = *it;
    nodes.erase(it);

    if (newIndex < 0)
      newIndex = 0;
    if (newIndex > (int)nodes.size())
      newIndex = (int)nodes.size();

    nodes.insert(nodes.begin() + newIndex, n);
    updateImplicitConnections();
  }
}

void GraphEngine::updateImplicitConnections() {
  // Clear all connections first
  for (auto &node : nodes) {
    node->clearConnections();
  }

  // Create implicit connections from node N output 0 to node N+1 input 0
  for (size_t i = 0; i + 1 < nodes.size(); ++i) {
    nodes[i]->addConnection(0, nodes[i + 1].get(), 0);
  }
}

void GraphEngine::recalculate() {
  // Linear processing for Step 7 (Implicit flow)
  // Each node processes using its input sequences which it gathers from
  // upstream

  for (size_t i = 0; i < nodes.size(); ++i) {
    // Before processing, we must copy the output of the previous node to our
    // input because we are using a linear Top-to-Bottom evaluation model.
    if (i > 0) {
      // Find all connections that point to us
      for (size_t j = 0; j < i; ++j) {
        for (const auto &connList : nodes[j]->getConnections()) {
          int outPort = connList.first;
          for (const auto &conn : connList.second) {
            if (conn.targetNode == nodes[i].get()) {
              nodes[i]->setInputSequence(conn.targetInputPort,
                                         nodes[j]->getOutputSequence(outPort));
            }
          }
        }
      }
    }
    nodes[i]->process();
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

      std::shared_ptr<GraphNode> sourceNode = nullptr;
      std::shared_ptr<GraphNode> targetNode = nullptr;

      for (const auto &node : nodes) {
        if (node->nodeId.toString() == sourceUuidStr)
          sourceNode = node;
        if (node->nodeId.toString() == targetUuidStr)
          targetNode = node;
      }

      if (sourceNode && targetNode) {
        // Temporarily disable clearing existing connections
        sourceNode->addConnection(sourcePort, targetNode.get(), targetPort);
      }
    }
  } else {
    // Fallback to implicit connections if no explicit routing found (Step 7
    // compat)
    updateImplicitConnections();
  }
}
