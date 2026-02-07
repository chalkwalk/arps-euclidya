#pragma once

#include "GraphNode.h"
#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>

class GraphCanvas; // forward declaration

class NodeBlock : public juce::Component {
public:
  NodeBlock(std::shared_ptr<GraphNode> node,
            juce::AudioProcessorValueTreeState &apvts, GraphCanvas &canvas);
  ~NodeBlock() override = default;

  void paint(juce::Graphics &g) override;
  void resized() override;

  // Mouse handling for dragging the node
  void mouseDown(const juce::MouseEvent &e) override;
  void mouseDrag(const juce::MouseEvent &e) override;
  void mouseUp(const juce::MouseEvent &e) override;

  std::shared_ptr<GraphNode> getNode() const { return targetNode; }

  // Get the centre point of a port in parent (canvas) coordinates
  juce::Point<int> getInputPortCentre(int portIndex) const;
  juce::Point<int> getOutputPortCentre(int portIndex) const;

  // Port geometry
  static constexpr int PORT_RADIUS = 8;
  static constexpr int PORT_HIT_RADIUS = 14; // generous hit area
  static constexpr int PORT_SPACING = 24;
  static constexpr int HEADER_HEIGHT = 28;
  static constexpr int PORT_MARGIN = 14;

  std::function<void()> onDelete;
  std::function<void()> onPositionChanged;

  // Callbacks for cable dragging (forwarded to canvas)
  std::function<void(NodeBlock *block, int portIndex, bool isOutput,
                     juce::Point<int> canvasPos)>
      onPortDragStart;
  std::function<void(juce::Point<int> canvasPos)> onPortDragging;
  std::function<void(juce::Point<int> canvasPos)> onPortDragEnd;

private:
  std::shared_ptr<GraphNode> targetNode;
  GraphCanvas &parentCanvas;

  juce::Label titleLabel;
  juce::TextButton deleteButton{"X"};
  std::unique_ptr<juce::Component> customControls;

  juce::ComponentDragger dragger;
  bool isDraggingNode = false;
  bool isDraggingCable = false;

  // Calculate the port rectangles for hit testing
  juce::Rectangle<int> getInputPortRect(int portIndex) const;
  juce::Rectangle<int> getOutputPortRect(int portIndex) const;
  int hitTestInputPort(juce::Point<int> localPoint) const;
  int hitTestOutputPort(juce::Point<int> localPoint) const;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NodeBlock)
};
