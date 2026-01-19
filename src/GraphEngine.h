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
  void moveNode(GraphNode *node, int newIndex);

  // Forces all nodes to recalculate their states downstream
  void recalculate();

  // Gets the current list of nodes (in order)
  const std::vector<std::shared_ptr<GraphNode>> &getNodes() const {
    return nodes;
  }

  // Rebuilds implicit connections based on node order and applies overrides
  void updateImplicitConnections();

  void saveState(juce::XmlElement *xmlRoot);
  void loadState(juce::XmlElement *xmlRoot, MidiHandler &midiCtx,
                 ClockManager &clockCtx,
                 std::array<std::atomic<float> *, 32> &macros);

private:
  std::vector<std::shared_ptr<GraphNode>> nodes;
};
