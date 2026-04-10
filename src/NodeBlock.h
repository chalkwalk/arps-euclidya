#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

#include "GraphNode.h"
#include "SharedMacroUI.h"

class GraphCanvas;  // forward declaration

class NodeBlock : public juce::Component, private juce::Timer {
 public:
  NodeBlock(const std::shared_ptr<GraphNode> &node,
            juce::AudioProcessorValueTreeState &apvts, GraphCanvas &canvas);
  ~NodeBlock() override { stopTimer(); }

  void paint(juce::Graphics &g) override;
  void resized() override;
  void timerCallback() override;

  // Mouse handling for dragging the node
  void mouseDown(const juce::MouseEvent &e) override;
  void mouseDrag(const juce::MouseEvent &e) override;
  void mouseUp(const juce::MouseEvent &e) override;

  // Aborts movement and reverts to start position
  void cancelDrag();

  [[nodiscard]] std::shared_ptr<GraphNode> getNode() const {
    return targetNode;
  }

  // Get the centre point of a port in parent (canvas) coordinates
  [[nodiscard]] juce::Point<int> getInputPortCentre(int portIndex) const;
  [[nodiscard]] juce::Point<int> getOutputPortCentre(int portIndex) const;

  // Port geometry
  static constexpr int PORT_RADIUS = 8;
  static constexpr int PORT_HIT_RADIUS = 14;  // generous hit area
  static constexpr int PORT_SPACING = 24;
  static constexpr int HEADER_HEIGHT = 28;
  static constexpr int PORT_MARGIN = 14;

  // Called by GraphCanvas after construction to share the editor's selection state
  void setSelectedMacroPtr(int *ptr) {
    selectedMacroPtr = ptr;
    // Propagate to all macro-aware sliders and buttons created during construction
    for (auto &info : sliderMacroInfos)
      if (auto *s = dynamic_cast<CustomMacroSlider *>(info.slider))
        s->selectedMacroPtr = ptr;
    for (auto *comp : dynamicComponents)
      if (auto *b = dynamic_cast<CustomMacroButton *>(comp))
        b->selectedMacroPtr = ptr;
    for (auto *comp : extendedComponents)
      if (auto *b = dynamic_cast<CustomMacroButton *>(comp))
        b->selectedMacroPtr = ptr;
  }

  std::function<void()> onDelete;
  std::function<void()> onPositionChanged;
  std::function<void(std::vector<int>)> onHoverMacros;      // forwarded from canvas → editor

  // Callbacks for cable dragging (forwarded to canvas)
  std::function<void(NodeBlock *block, int portIndex, bool isOutput,
                     juce::Point<int> canvasPos)>
      onPortDragStart;
  std::function<void(juce::Point<int> canvasPos)> onPortDragging;
  std::function<void(juce::Point<int> canvasPos)> onPortDragEnd;

  // Called when the node is clicked/selected
  std::function<void()> onSelected;
  // Called on shift-click: toggles node in/out of the multi-selection
  std::function<void()> onAddToSelection;
  std::function<void(const juce::String &)> onReplaceRequest;

 private:
  std::shared_ptr<GraphNode> targetNode;
  GraphCanvas &parentCanvas;

  juce::Label titleLabel;
  juce::TextButton deleteButton{"X"};
  juce::TextButton bypassButton{"B"};
  juce::TextButton expandButton{">"};

  bool isExpanded = false;

  void toggleExpansion();
  void updateSize();

  // Fallback for nodes not yet migrated to NodeLayout
  std::unique_ptr<juce::Component> customControls;

  // Dynamic UI Elements (Layout-driven)
  juce::OwnedArray<juce::Component> dynamicComponents;
  juce::OwnedArray<juce::Component> extendedComponents;

  // Selected-macro state (pointer into editor's selectedMacro field)
  int *selectedMacroPtr = nullptr;
  int lastKnownSelectedMacro = -1;

  // Macro index being hovered in the palette (-1 = none)
  int highlightedMacroIndex = -1;

 public:
  void setHighlightedMacro(int idx) {
    highlightedMacroIndex = idx;
    repaint();
  }

 private:

  // Tracks sliders that have a macroParamRef, for overlay rendering
  struct SliderMacroInfo {
    juce::Slider *slider;
    MacroParam *macroParamRef;
  };
  std::vector<SliderMacroInfo> sliderMacroInfos;

  // Tracks buttons that have a macroParamRef, for overlay rendering
  struct ButtonMacroInfo {
    CustomMacroButton *button;
    MacroParam *macroParamRef;
    int *valueRef;  // pointer to the node's local int value (may be nullptr)
  };
  std::vector<ButtonMacroInfo> buttonMacroInfos;

  void paintOverChildren(juce::Graphics &g) override;

  int dragStartGridX = 0;
  int dragStartGridY = 0;
  float dragStartWorldX = 0.0f;
  float dragStartWorldY = 0.0f;

  // True when mouseDown saw a node already in a multi-selection.
  // We defer the selection change to mouseUp so drag can start group drag
  // without clearing the selection first.
  bool wasInGroupSelection = false;

  juce::ComponentDragger dragger;
  bool isDraggingNode = false;
  bool isDraggingCable = false;

  // Calculate the port rectangles for hit testing
  [[nodiscard]] static juce::Rectangle<int> getInputPortRect(int portIndex);
  [[nodiscard]] juce::Rectangle<int> getOutputPortRect(int portIndex) const;
  [[nodiscard]] int hitTestInputPort(juce::Point<int> localPoint) const;
  [[nodiscard]] int hitTestOutputPort(juce::Point<int> localPoint) const;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NodeBlock)
};

/**
 * A lightweight preview of a node used during dragging from the library.
 * Mimics the NodeBlock visual style.
 */
class NodeDragPreview : public juce::Component {
 public:
  NodeDragPreview(juce::String nodeType, int gridW, int gridH, int numIn,
                  int numOut);

  void paint(juce::Graphics &g) override;

  [[nodiscard]] int getGridWidth() const { return gridW; }
  [[nodiscard]] int getGridHeight() const { return gridH; }

 private:
  juce::String type;
  int gridW, gridH, numIn, numOut;
};
