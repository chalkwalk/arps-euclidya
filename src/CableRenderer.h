#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <optional>
#include <unordered_set>
#include <vector>

#include "GraphEngine.h"

struct CachedCable {
  GraphNode *sourceNode;
  int sourcePort;
  GraphNode *targetNode;
  int targetPort;
  juce::Path path;
  bool isLarge = false;
  bool isSelected = false;
  int stepCount = 0;
  int activeStepCount = 0;
  GraphNode::PortType portType = GraphNode::PortType::Notes;
};

struct CableID {
  GraphNode *sourceNode = nullptr;
  int sourcePort = 0;
  GraphNode *targetNode = nullptr;
  int targetPort = 0;

  [[nodiscard]] bool isValid() const {
    return sourceNode != nullptr && targetNode != nullptr;
  }
  void clear() {
    sourceNode = nullptr;
    targetNode = nullptr;
  }
  [[nodiscard]] bool matches(const CachedCable &c) const {
    return sourceNode == c.sourceNode && sourcePort == c.sourcePort &&
           targetNode == c.targetNode && targetPort == c.targetPort;
  }
};

class CableRenderer {
 public:
  // getPortPos(node, portIndex, isOutput) → world position, or nullopt if the
  // NodeBlock for that node doesn't exist yet.
  using PortPosFn = std::function<std::optional<juce::Point<int>>(
      GraphNode *, int, bool isOutput)>;

  CableRenderer(GraphEngine &engine, juce::CriticalSection &lock,
                PortPosFn getPortPos);

  void refresh(const std::unordered_set<GraphNode *> &selectedNodes);

  [[nodiscard]] const std::vector<CachedCable> &getCables() const {
    return cachedCables;
  }

  [[nodiscard]] bool hasLargeSequence() const;

  void drawCable(juce::Graphics &g, const juce::Path &path, bool highlighted,
                 bool warning, bool isForeground, GraphNode::PortType portType,
                 bool hasSelection) const;

  // Scan the graph directly for >10000-step outputs; returns true if found.
  bool checkForLargeSequences();

 private:
  GraphEngine &graphEngine;
  juce::CriticalSection &graphLock;
  PortPosFn getPortPos;
  std::vector<CachedCable> cachedCables;
};
