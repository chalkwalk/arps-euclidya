#pragma once

#include "GraphEngine.h"
#include "NodeBlock.h"
#include <juce_gui_basics/juce_gui_basics.h>

class GraphCanvas : public juce::Component {
public:
  GraphCanvas(GraphEngine &engine, juce::AudioProcessorValueTreeState &apvts,
              juce::CriticalSection &lock);
  ~GraphCanvas() override = default;

  void paint(juce::Graphics &g) override;
  void paintOverChildren(juce::Graphics &g) override;

  // Mouse handling for right-click cable deletion and hover tooltips
  void mouseDown(const juce::MouseEvent &e) override;
  void mouseMove(const juce::MouseEvent &e) override;
  void mouseExit(const juce::MouseEvent &e) override;

  // Rebuild all NodeBlocks from the engine's node list
  void rebuild();

  // Add a node and place it at a default position
  void addNodeAtDefaultPosition(std::shared_ptr<GraphNode> node);

  // Called by NodeBlock when a port drag starts/moves/ends
  void startCableDrag(NodeBlock *block, int portIndex, bool isOutput,
                      juce::Point<int> canvasPos);
  void updateCableDrag(juce::Point<int> canvasPos);
  void endCableDrag(juce::Point<int> canvasPos);

  // Check if any cable carries a sequence >10K steps
  void checkForLargeSequences();

  // Select a node (bring to front and highlight its cables)
  void selectNode(GraphNode *node);
  GraphNode *getSelectedNode() const { return selectedNode; }

  // Callback when the graph structure changes
  std::function<void()> onGraphChanged;

private:
  GraphEngine &graphEngine;
  juce::AudioProcessorValueTreeState &apvts;
  juce::CriticalSection &graphLock;

  juce::OwnedArray<NodeBlock> nodeBlocks;

  // Cable dragging state
  bool isDraggingCable = false;
  NodeBlock *cableDragSourceBlock = nullptr;
  int cableDragSourcePort = -1;
  bool cableDragFromOutput = true;
  juce::Point<int> cableDragEnd;

  NodeBlock *findBlockForNode(GraphNode *node) const;
  void drawCable(juce::Graphics &g, juce::Point<int> start,
                 juce::Point<int> end, bool highlighted = false,
                 bool warning = false, bool selected = false);
  void updateCanvasSize();

  // Cable tooltip state
  juce::String cableTooltipText;
  juce::Point<int> cableTooltipPos;
  bool showCableTooltip = false;

  // Warning banner
  bool hasLargeSequenceWarning = false;

  // Selected node for highlighting
  GraphNode *selectedNode = nullptr;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GraphCanvas)
};
