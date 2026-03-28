#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "GraphEngine.h"
#include "NodeBlock.h"

class GraphCanvas : public juce::Component,
                    public juce::ScrollBar::Listener,
                    public juce::DragAndDropTarget,
                    public juce::KeyListener {
 public:
  GraphCanvas(GraphEngine &engine, juce::AudioProcessorValueTreeState &apvts,
              juce::CriticalSection &lock);
  ~GraphCanvas() override = default;

  void paint(juce::Graphics &g) override;
  void paintOverChildren(juce::Graphics &g) override;
  void resized() override;
  void scrollBarMoved(juce::ScrollBar *scrollBarThatHasMoved,
                      double newRangeStart) override;

  // Mouse handling for right-click cable deletion and hover tooltips
  void mouseDown(const juce::MouseEvent &e) override;
  void mouseMove(const juce::MouseEvent &e) override;
  void mouseDrag(const juce::MouseEvent &e) override;
  void mouseUp(const juce::MouseEvent &e) override;
  void mouseExit(const juce::MouseEvent &e) override;
  void mouseWheelMove(const juce::MouseEvent &e,
                      const juce::MouseWheelDetails &wheel) override;

  bool keyPressed(const juce::KeyPress &key,
                  juce::Component *originatingComponent) override;

  // Zoom manipulation
  void setZoomFactor(float newZoom);
  float getZoomFactor() const { return zoomFactor; }

  // Rebuild all NodeBlocks from the engine's node list
  void rebuild();

  GraphEngine &getEngine() const { return graphEngine; }

  // Add a node and place it at a default position
  void addNodeAtDefaultPosition(std::shared_ptr<GraphNode> node);

  // Add a node explicitly converting screen coordinates to world position
  void addNodeAtPosition(std::shared_ptr<GraphNode> node,
                         juce::Point<int> screenPos);

  // DragAndDropTarget overrides
  bool isInterestedInDragSource(
      const SourceDetails &dragSourceDetails) override;
  void itemDragMove(const SourceDetails &dragSourceDetails) override;
  void itemDragExit(const SourceDetails &dragSourceDetails) override;
  void itemDropped(const SourceDetails &dragSourceDetails) override;

  // Called by NodeBlock when a port drag starts/moves/ends
  void startCableDrag(NodeBlock *block, int portIndex, bool isOutput,
                      juce::Point<int> canvasPos);
  void updateCableDrag(juce::Point<int> canvasPos);
  void endCableDrag(juce::Point<int> canvasPos);

  // Called by NodeBlock to request a clone
  void requestNodeClone(GraphNode *original, int gridX, int gridY);

  // Check if any cable carries a sequence >10K steps
  void checkForLargeSequences();

  // Select a node (bring to front and highlight its cables)
  void selectNode(GraphNode *node);
  GraphNode *getSelectedNode() const { return selectedNode; }

  // Callback when the graph structure changes
  std::function<void()> onGraphChanged;

  // Ghost Rendering API for Node Dragging Preview
  void setGhostTarget(int gridX, int gridY, int gridW, int gridH,
                      GraphNode *ignoreNode);
  void clearGhostTarget();
  int getGhostX() const { return ghostTargetX; }
  int getGhostY() const { return ghostTargetY; }
  bool isGhostValid() const { return ghostIsValid; }

  // Proximity auto-connection & Signal Path Insertion
  void attemptProximityConnection(GraphNode *droppedNode,
                                  juce::Point<int> mousePos);
  bool attemptSignalPathInsertion(GraphNode *newNode,
                                  juce::Point<int> mousePos);

  enum class ProximityZone { None, Left, Right };
  void updateProximityHighlight(juce::Point<int> mousePos,
                                GraphNode *draggingNode);
  void clearProximityHighlight();
  GraphNode *getProximityTargetNode() const { return proximityTargetNode; }
  ProximityZone getProximityZone() const { return proximityZone; }

  // Callbacks for editor/processor
  std::function<void(const juce::String &, juce::Point<int>)> onNodeDropped;
  std::function<void(GraphNode *, int, int)> onNodeCloneRequest;

  // Refresh the cached cable paths
  void refreshCableCache();

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
  void drawCable(juce::Graphics &g, const juce::Path &path, bool isDragging,
                 bool isHighlighted, bool isOutputPortConnected);
  void updateCanvasSize();

  // Cable tooltip state
  juce::String cableTooltipText;
  juce::Point<int> cableTooltipPos;
  bool showCableTooltip = false;

  // Camera Projection
  bool isPanning = false;
  juce::Point<int> lastPanScreenPos;
  float panX = 0.0f;
  float panY = 0.0f;
  float zoomFactor = 1.0f;

  juce::ScrollBar hScroll{false};
  juce::ScrollBar vScroll{true};
  juce::TextButton zoomFitButton;

  juce::AffineTransform getCameraTransform() const;
  void updateTransforms();
  void updateScrollBars();
  void zoomToFit();

  // Warning banner
  bool hasLargeSequenceWarning = false;

  // Ghost target rendering state
  bool showGhostTarget = false;
  int ghostTargetX = 0;
  int ghostTargetY = 0;
  int ghostTargetW = 1;
  int ghostTargetH = 1;
  bool ghostIsValid = false;

  struct CachedCable {
    GraphNode *sourceNode;
    int sourcePort;
    GraphNode *targetNode;
    int targetPort;
    juce::Path path;
    bool isLarge = false;
    bool isSelected = false;
  };

  struct CableID {
    GraphNode *sourceNode = nullptr;
    int sourcePort = 0;
    GraphNode *targetNode = nullptr;
    int targetPort = 0;

    bool isValid() const {
      return sourceNode != nullptr && targetNode != nullptr;
    }
    void clear() {
      sourceNode = nullptr;
      targetNode = nullptr;
    }
    bool matches(const CachedCable &c) const {
      return sourceNode == c.sourceNode && sourcePort == c.sourcePort &&
             targetNode == c.targetNode && targetPort == c.targetPort;
    }
  };

  // Selection & Highlight state
  GraphNode *selectedNode = nullptr;
  GraphNode *proximityTargetNode = nullptr;
  ProximityZone proximityZone = ProximityZone::None;
  CableID proximityCableID;

  std::vector<CachedCable> cachedCables;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GraphCanvas)
};
