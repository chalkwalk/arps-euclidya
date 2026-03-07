#include "NodeBlock.h"
#include "GraphCanvas.h"
#include "GraphEngine.h"

NodeBlock::NodeBlock(std::shared_ptr<GraphNode> node,
                     juce::AudioProcessorValueTreeState &apvts,
                     GraphCanvas &canvas)
    : targetNode(node), parentCanvas(canvas) {

  titleLabel.setText(node->getName(), juce::dontSendNotification);
  titleLabel.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
  titleLabel.setJustificationType(juce::Justification::centredLeft);
  titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
  titleLabel.setInterceptsMouseClicks(false, false);
  addAndMakeVisible(titleLabel);

  deleteButton.setColour(juce::TextButton::buttonColourId,
                         juce::Colour(0xff882222));
  addAndMakeVisible(deleteButton);
  deleteButton.onClick = [this] {
    // Defer deletion: rebuild() destroys nodeBlocks (including this NodeBlock)
    // so we must not do it while this button's click handler is on the stack.
    juce::MessageManager::callAsync([cb = onDelete]() {
      if (cb)
        cb();
    });
  };

  customControls = node->createEditorComponent(apvts);
  if (customControls != nullptr) {
    addAndMakeVisible(customControls.get());
  }

  // Physical bounds derived exclusively from grid properties
  int gridW = node->getGridWidth();
  int gridH = node->getGridHeight();

  // Tram lines math: Width is W*100 - 10, Height is H*100 - 10
  int width = (gridW * 100) - 10;
  int height = (gridH * 100) - 10;

  setSize(width, height);
}

void NodeBlock::paint(juce::Graphics &g) {
  auto bounds = getLocalBounds().toFloat();

  // Node body: rounded rectangle
  g.setColour(juce::Colour(0xff121a24));
  g.fillRoundedRectangle(bounds, 6.0f);

  // Header bar
  auto headerBounds =
      juce::Rectangle<float>(bounds.getX() + 1, bounds.getY() + 1,
                             bounds.getWidth() - 2, (float)HEADER_HEIGHT - 1);
  g.setColour(juce::Colour(0xff1d2a3a));
  g.fillRoundedRectangle(headerBounds.getX(), headerBounds.getY(),
                         headerBounds.getWidth(), headerBounds.getHeight(),
                         5.0f);

  // Border (Neon tinted)
  g.setColour(juce::Colour(0xff0df0e3).withAlpha(0.3f));
  g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 6.0f, 1.5f);

  // Draw input ports
  int numIn = targetNode->getNumInputPorts();
  for (int i = 0; i < numIn; ++i) {
    auto rect = getInputPortRect(i);
    auto centre = rect.getCentre().toFloat();

    // Filled green circle
    g.setColour(juce::Colour(0xff44cc44));
    g.fillEllipse(centre.x - PORT_RADIUS, centre.y - PORT_RADIUS,
                  PORT_RADIUS * 2.0f, PORT_RADIUS * 2.0f);
    g.setColour(juce::Colour(0xff228822));
    g.drawEllipse(centre.x - PORT_RADIUS, centre.y - PORT_RADIUS,
                  PORT_RADIUS * 2.0f, PORT_RADIUS * 2.0f, 1.5f);
  }

  // Draw output ports
  int numOut = targetNode->getNumOutputPorts();
  for (int i = 0; i < numOut; ++i) {
    auto rect = getOutputPortRect(i);
    auto centre = rect.getCentre().toFloat();

    // Filled blue circle
    g.setColour(juce::Colour(0xff4499ff));
    g.fillEllipse(centre.x - PORT_RADIUS, centre.y - PORT_RADIUS,
                  PORT_RADIUS * 2.0f, PORT_RADIUS * 2.0f);
    g.setColour(juce::Colour(0xff2266aa));
    g.drawEllipse(centre.x - PORT_RADIUS, centre.y - PORT_RADIUS,
                  PORT_RADIUS * 2.0f, PORT_RADIUS * 2.0f, 1.5f);
  }
}

void NodeBlock::resized() {
  auto bounds = getLocalBounds();

  // Header
  auto header = bounds.removeFromTop(HEADER_HEIGHT).reduced(PORT_MARGIN + 4, 2);
  deleteButton.setBounds(header.removeFromRight(24));
  header.removeFromRight(4);
  titleLabel.setBounds(header);

  // Body: custom controls
  if (customControls != nullptr) {
    auto body = bounds.reduced(PORT_MARGIN + 4, 4);
    customControls->setBounds(body);
  }
}

void NodeBlock::mouseDown(const juce::MouseEvent &e) {
  if (e.mods.isMiddleButtonDown()) {
    return;
  }

  // Notify canvas of selection (for z-ordering and cable highlighting)
  if (onSelected)
    onSelected();

  auto pos = e.getPosition();

  // Check if we hit a port
  int outPort = hitTestOutputPort(pos);
  if (outPort >= 0) {
    isDraggingCable = true;
    isDraggingNode = false;
    if (onPortDragStart) {
      auto canvasPos = e.getEventRelativeTo(&parentCanvas).getPosition();
      onPortDragStart(this, outPort, true, canvasPos);
    }
    return;
  }

  int inPort = hitTestInputPort(pos);
  if (inPort >= 0) {
    isDraggingCable = true;
    isDraggingNode = false;
    if (onPortDragStart) {
      auto canvasPos = e.getEventRelativeTo(&parentCanvas).getPosition();
      onPortDragStart(this, inPort, false, canvasPos);
    }
    return;
  }

  // Otherwise, start dragging the node
  isDraggingNode = true;
  isDraggingCable = false;

  // No longer using standard ComponentDragger since we do strict grid hopping
  // But we store the starting point of the drag
  dragStartGridX = targetNode->gridX;
  dragStartGridY = targetNode->gridY;
}

void NodeBlock::mouseDrag(const juce::MouseEvent &e) {
  if (e.mods.isMiddleButtonDown()) {
    return;
  }

  if (isDraggingCable) {
    if (onPortDragging) {
      auto canvasPos = e.getEventRelativeTo(&parentCanvas).getPosition();
      onPortDragging(canvasPos);
    }
    return;
  }

  if (isDraggingNode) {
    // Determine the raw float destination based on the mouse stroke
    auto desktopPos = e.getEventRelativeTo(&parentCanvas).getPosition();

    // Abstract world coordinates mapped from the canvas
    float worldX =
        (float)desktopPos.x / parentCanvas.getZoomFactor() -
        (parentCanvas.getBounds().getWidth() / parentCanvas.getZoomFactor() *
         0); // Not needed, just use component distance

    // Since dealing with camera transforms is complex here, let's just
    // accumulate the total drag vector from the mouse down event
    int deltaX = e.getDistanceFromDragStartX() / parentCanvas.getZoomFactor();
    int deltaY = e.getDistanceFromDragStartY() / parentCanvas.getZoomFactor();

    // Convert to grid slots: roughly 100px per slot
    int gridDeltaX = (int)std::round((float)deltaX / 100.0f);
    int gridDeltaY = (int)std::round((float)deltaY / 100.0f);

    int newGridX = dragStartGridX + gridDeltaX;
    int newGridY = dragStartGridY + gridDeltaY;

    if (newGridX != targetNode->gridX || newGridY != targetNode->gridY) {
      // Check collision
      if (!parentCanvas.getEngine().isAreaOccupied(
              newGridX, newGridY, targetNode->getGridWidth(),
              targetNode->getGridHeight(), targetNode.get())) {
        targetNode->gridX = newGridX;
        targetNode->gridY = newGridY;

        // Ensure legacy float sync keeps the block in the right spot physically
        targetNode->nodeX = (float)(newGridX * 100);
        targetNode->nodeY = (float)(newGridY * 100);

        if (onPositionChanged)
          onPositionChanged();
      }
    }
  }
}

void NodeBlock::mouseUp(const juce::MouseEvent &e) {
  if (e.mods.isMiddleButtonDown()) {
    return;
  }

  if (isDraggingCable && onPortDragEnd) {
    auto canvasPos = e.getEventRelativeTo(&parentCanvas).getPosition();
    onPortDragEnd(canvasPos);
  }
  isDraggingCable = false;
  isDraggingNode = false;
}

juce::Rectangle<int> NodeBlock::getInputPortRect(int portIndex) const {
  int y = HEADER_HEIGHT + 10 + portIndex * PORT_SPACING;
  return juce::Rectangle<int>(PORT_MARGIN - PORT_RADIUS, y - PORT_RADIUS,
                              PORT_RADIUS * 2, PORT_RADIUS * 2);
}

juce::Rectangle<int> NodeBlock::getOutputPortRect(int portIndex) const {
  int y = HEADER_HEIGHT + 10 + portIndex * PORT_SPACING;
  return juce::Rectangle<int>(getWidth() - PORT_MARGIN - PORT_RADIUS,
                              y - PORT_RADIUS, PORT_RADIUS * 2,
                              PORT_RADIUS * 2);
}

juce::Point<int> NodeBlock::getInputPortCentre(int portIndex) const {
  auto rect = getInputPortRect(portIndex);
  return getPosition() + rect.getCentre();
}

juce::Point<int> NodeBlock::getOutputPortCentre(int portIndex) const {
  auto rect = getOutputPortRect(portIndex);
  return getPosition() + rect.getCentre();
}

int NodeBlock::hitTestInputPort(juce::Point<int> localPoint) const {
  int numIn = targetNode->getNumInputPorts();
  auto localFloat = localPoint.toFloat();
  for (int i = 0; i < numIn; ++i) {
    auto rect = getInputPortRect(i);
    auto centre = rect.getCentre().toFloat();
    if (localFloat.getDistanceFrom(centre) <= PORT_HIT_RADIUS)
      return i;
  }
  return -1;
}

int NodeBlock::hitTestOutputPort(juce::Point<int> localPoint) const {
  int numOut = targetNode->getNumOutputPorts();
  auto localFloat = localPoint.toFloat();
  for (int i = 0; i < numOut; ++i) {
    auto rect = getOutputPortRect(i);
    auto centre = rect.getCentre().toFloat();
    if (localFloat.getDistanceFrom(centre) <= PORT_HIT_RADIUS)
      return i;
  }
  return -1;
}
