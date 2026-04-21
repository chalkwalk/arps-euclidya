// GraphEngine.h
#pragma once

#include <array>
#include <atomic>
#include <memory>
#include <unordered_set>
#include <vector>

#include "GraphNode.h"

class NoteExpressionManager;
class ClockManager;

class GraphEngine {
 public:
  GraphEngine() = default;
  ~GraphEngine() = default;

  void addNode(const std::shared_ptr<GraphNode> &node);
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
  bool isAreaOccupied(int gridX, int gridY, int gridW, int gridH,
                      const std::unordered_set<GraphNode *> &ignoreSet) const;

  // Returns true when the 1-cell border around node is clear of other nodes.
  [[nodiscard]] bool isUnfoldFootprintClear(const GraphNode *node) const;

  // Helpers to get specific node types
  std::vector<class MidiOutNode *> getMidiOutNodes() const;
  std::vector<class QuantizerNode *> getQuantizerNodes() const;

  juce::Point<int> findClosestFreeSpot(int startX, int startY, int gridW,
                                       int gridH,
                                       GraphNode *ignoreNode = nullptr,
                                       juce::Point<int> preferredPoint = {
                                           -1, -1}) const;

  int getNextFreeMacro() const;

  std::function<void()> onGraphDirtied;

  // Fired when the structural topology changes (connections added/removed)
  // so the audio thread knows to force-push data through the new lines
  std::function<void()> onTopologyChanged;

  // Occupancy checks
  bool isInputPortOccupied(GraphNode *node, int inPort) const;
  static bool isOutputPortOccupied(GraphNode *node, int portIndex);

  // Check if adding a connection would create a cycle
  bool wouldCreateCycle(GraphNode *source, GraphNode *target) const;

  // Port type compatibility — used by GUI for rejection flash and by
  // addExplicitConnection as a safety guard
  bool checkPortTypeCompatibility(GraphNode *source, int outPort,
                                  GraphNode *target, int inPort) const;
  GraphNode::PortType getEffectiveOutputPortType(GraphNode *node,
                                                 int port) const;
  GraphNode::PortType getEffectiveInputPortType(GraphNode *node,
                                                int port) const;

  void saveState(juce::XmlElement *xmlRoot);
  void loadState(juce::XmlElement *xmlRoot, NoteExpressionManager &midiCtx,
                 ClockManager &clockCtx,
                 std::array<std::atomic<float> *, 32> &macros);

  void setMacros(std::array<std::atomic<float> *, 32> *m) { macrosPtr = m; }

 private:
  std::vector<std::shared_ptr<GraphNode>> nodes;
  std::array<std::atomic<float> *, 32> *macrosPtr = nullptr;

  // Compute a topological ordering of nodes based on connections
  std::vector<GraphNode *> topologicalSort() const;

  // Helper: check if 'target' is reachable from 'source' via connections
  static bool isReachable(GraphNode *from, GraphNode *to);
};
