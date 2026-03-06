// GraphEngine.h
#pragma once

#include "GraphNode.h"
#include <memory>
#include <vector>

class MidiHandler;
class ClockManager;

class GraphEngine {
public:
  GraphEngine() = default;
  ~GraphEngine() = default;

  void addNode(std::shared_ptr<GraphNode> node);
  void removeNode(GraphNode *node);

  // Explicit connection management (replaces implicit linear chain)
  bool addExplicitConnection(GraphNode *source, int outPort, GraphNode *target,
                             int inPort);
  void removeConnection(GraphNode *source, int outPort, GraphNode *target,
                        int inPort);

  // Forces all nodes to recalculate their states downstream (topological order)
  void recalculate();

  // Gets the current list of nodes
  const std::vector<std::shared_ptr<GraphNode>> &getNodes() const {
    return nodes;
  }

  // Grid collision detection
  bool isAreaOccupied(int gridX, int gridY, int gridW, int gridH,
                      GraphNode *ignoreNode = nullptr) const;

  std::function<void()> onGraphDirtied;

  // Check if adding a connection would create a cycle
  bool wouldCreateCycle(GraphNode *source, GraphNode *target) const;

  void saveState(juce::XmlElement *xmlRoot);
  void loadState(juce::XmlElement *xmlRoot, MidiHandler &midiCtx,
                 ClockManager &clockCtx,
                 std::array<std::atomic<float> *, 32> &macros);

private:
  std::vector<std::shared_ptr<GraphNode>> nodes;

  // Compute a topological ordering of nodes based on connections
  std::vector<GraphNode *> topologicalSort() const;

  // Helper: check if 'target' is reachable from 'source' via connections
  bool isReachable(GraphNode *source, GraphNode *target) const;
};
