#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <unordered_set>

#include "GraphEngine.h"
#include "NodeBlock.h"

class GraphCanvas : public juce::Component,
                    public juce::ScrollBar::Listener,
                    public juce::DragAndDropTarget,
                    public juce::KeyListener,
                    public juce::Timer {
 public:
  GraphCanvas(GraphEngine &engine, juce::AudioProcessorValueTreeState &apvts,
              juce::CriticalSection &lock);
  ~GraphCanvas() override = default;

  void undo();
  void redo();

  std::function<void(std::function<void()>)> performMutation;
  std::function<void()> onUndo;
  std::function<void()> onRedo;

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

  using juce::Component::keyPressed;
  bool keyPressed(const juce::KeyPress &key,
                  juce::Component *originatingComponent) override;

  // Zoom manipulation
  void setZoomFactor(float newZoom);
  float getZoomFactor() const { return zoomFactor; }

  // Rebuild all NodeBlocks from the engine's node list
  void rebuild();

  GraphEngine &getEngine() const { return graphEngine; }

  // Add a node and place it at a default position
  void addNodeAtDefaultPosition(const std::shared_ptr<GraphNode> &node);

  // Add a node explicitly converting screen coordinates to world position
  void addNodeAtPosition(const std::shared_ptr<GraphNode> &node,
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
  void requestNodeClone(GraphNode *original, int gridX, int gridY) const;

  // Check if any cable carries a sequence >10K steps
  void checkForLargeSequences();

  // Selection API
  void selectNode(GraphNode *node);                // clear set, select one node
  void addToSelection(GraphNode *node);            // toggle node in selection
  void clearSelection();                            // deselect everything
  [[nodiscard]] GraphNode *getSelectedNode() const {
    return selectedNodes.size() == 1 ? *selectedNodes.begin() : nullptr;
  }
  [[nodiscard]] bool isNodeSelected(GraphNode *node) const {
    return selectedNodes.count(node) > 0;
  }
  [[nodiscard]] int getSelectionSize() const {
    return static_cast<int>(selectedNodes.size());
  }

  // Callback when the graph structure changes
  std::function<void()> onGraphChanged;

  // Group drag API — called from NodeBlock when multiple nodes are selected
  void beginGroupDrag(GraphNode *anchor, const juce::MouseEvent &e);
  void updateGroupDrag(const juce::MouseEvent &e);
  void commitGroupDrag(bool isClone);
  void cancelGroupDrag();
  [[nodiscard]] bool isGroupDragging() const { return isGroupDragging_; }

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
  std::function<void(GraphNode *, const juce::String &)> onNodeReplaceRequest;
  std::function<void(GraphNode *, int, int)> onNodeCloneRequest;
  std::function<void()> onTransportToggle;

  // Refresh the cached cable paths
  void refreshCableCache();

  // Share the editor's macro selection state with the canvas and all NodeBlocks
  void setSelectedMacroPtr(int *ptr);

  // Highlight controls bound to the given macro index (-1 to clear)
  void setHighlightedMacro(int macroIndex);

  // Forwarded from editor → canvas → NodeBlock → CustomMacroSlider/Button/ComboBox
  std::function<void(std::vector<int>)> onHoverMacros;
  std::function<void(MacroParam *)> onRequestMidiLearn;
  std::function<void()> onCancelMidiLearn;

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
  void drawCable(juce::Graphics &g, const juce::Path &path, bool highlighted,
                 bool warning, bool isForeground,
                 GraphNode::PortType portType);
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
  juce::Point<int> getViewportGridCenter() const;
  void updateTransforms();
  void updateScrollBars();
  void zoomToFit();

  // Warning banner
  bool hasLargeSequenceWarning = false;

  // Reject flash (type-mismatch cable drop feedback)
  float rejectFlashAlpha = 0.0f;
  juce::Point<float> rejectFlashPos;
  void timerCallback() override;

  // Ghost target rendering state
  bool showGhostTarget = false;
  int ghostTargetX = 0;
  int ghostTargetY = 0;
  int ghostTargetW = 1;
  int ghostTargetH = 1;
  int ghostResolvedX = 0;
  int ghostResolvedY = 0;
  bool ghostIsValid = false;
  bool hasGhostResolved = false;

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

  // Macro selection state (pointer into editor's selectedMacro field)
  int *selectedMacroPtr = nullptr;

  // Selection & Highlight state
  std::unordered_set<GraphNode *> selectedNodes;
  GraphNode *proximityTargetNode = nullptr;
  ProximityZone proximityZone = ProximityZone::None;
  CableID proximityCableID;

  // Box-selection (rubber-band) state
  bool isBoxSelecting = false;
  juce::Point<float> boxSelectStartScreen;
  juce::Point<float> boxSelectCurrentScreen;

  // Group drag state
  bool isGroupDragging_ = false;
  GraphNode *groupDragAnchor = nullptr;
  struct GroupDragNodeData {
    GraphNode *node;
    int startGridX;
    int startGridY;
    float startWorldX;
    float startWorldY;
  };
  std::vector<GroupDragNodeData> groupDragNodes;
  int groupDragBBoxMinX = 0;
  int groupDragBBoxMinY = 0;
  int groupDragBBoxW = 0;
  int groupDragBBoxH = 0;
  int groupDragCurrentDeltaX = 0;
  int groupDragCurrentDeltaY = 0;

  void setGroupGhostTarget(int bboxGridX, int bboxGridY);

  std::vector<CachedCable> cachedCables;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GraphCanvas)
};
