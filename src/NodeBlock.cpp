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
    customControls->setInterceptsMouseClicks(false, true);
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

  // Unified Node body: rounded rectangle (no distinct header bar anymore)
  g.setColour(juce::Colour(0xff121a24));
  g.fillRoundedRectangle(bounds, 6.0f);

  // Border (Neon tinted)
  g.setColour(juce::Colour(0xff0df0e3).withAlpha(0.3f));
  g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 6.0f, 1.5f);

  // Draw input ports (Half stadiums flush with left edge)
  int numIn = targetNode->getNumInputPorts();
  for (int i = 0; i < numIn; ++i) {
    auto rect = getInputPortRect(i).toFloat();

    // Half-stadium path (rounded on the right, flat on the left)
    juce::Path p;
    p.addRoundedRectangle(rect.getX() - rect.getWidth(), rect.getY(),
                          rect.getWidth() * 2.0f, rect.getHeight(),
                          rect.getHeight() * 0.5f);

    g.saveState();
    g.reduceClipRegion(rect.toNearestInt()); // Clip to just the right half

    g.setColour(juce::Colour(0xff44cc44));
    g.fillPath(p);
    g.setColour(juce::Colour(0xff228822));
    g.strokePath(p, juce::PathStrokeType(1.5f));

    g.restoreState();
  }

  // Draw output ports (Half stadiums flush with right edge)
  int numOut = targetNode->getNumOutputPorts();
  for (int i = 0; i < numOut; ++i) {
    auto rect = getOutputPortRect(i).toFloat();

    // Half-stadium path (rounded on the left, flat on the right)
    juce::Path p;
    p.addRoundedRectangle(rect.getX(), rect.getY(), rect.getWidth() * 2.0f,
                          rect.getHeight(), rect.getHeight() * 0.5f);

    g.saveState();
    g.reduceClipRegion(rect.toNearestInt()); // Clip to just the left half

    g.setColour(juce::Colour(0xff4499ff));
    g.fillPath(p);
    g.setColour(juce::Colour(0xff2266aa));
    g.strokePath(p, juce::PathStrokeType(1.5f));

    g.restoreState();
  }
}

void NodeBlock::resized() {
  auto bounds = getLocalBounds();

  // Floating Header Area (same spacing, just no background drawn)
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

  parentCanvas.setGhostTarget(dragStartGridX, dragStartGridY,
                              targetNode->getGridWidth(),
                              targetNode->getGridHeight(), targetNode.get());
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
    // Deltas are already local to the component, meaning they are unscaled!
    // We do NOT need to divide by zoomFactor again.
    float cx =
        (float)e.getDistanceFromDragStartX() / parentCanvas.getZoomFactor();
    float cy =
        (float)e.getDistanceFromDragStartY() / parentCanvas.getZoomFactor();

    // Actually, juce::MouseEvent::getDistanceFromDragStartX() is measured in
    // the component's local coordinates. When the component is scaled via
    // AffineTransform, its local coordinate system remains constant (e.g.,
    // width is always 190). So the distance drag is exactly how many abstract
    // pixels we've moved.
    int deltaX = e.getDistanceFromDragStartX();
    int deltaY = e.getDistanceFromDragStartY();

    // Convert to grid slots: roughly 100px per slot
    int gridDeltaX = (int)std::round((float)deltaX / 100.0f);
    int gridDeltaY = (int)std::round((float)deltaY / 100.0f);

    int newGridX = dragStartGridX + gridDeltaX;
    int newGridY = dragStartGridY + gridDeltaY;

    if (newGridX != parentCanvas.getGhostX() ||
        newGridY != parentCanvas.getGhostY()) {
      // Set ghost target; parentCanvas handles the overlap check to tint it
      // red/green
      parentCanvas.setGhostTarget(
          newGridX, newGridY, targetNode->getGridWidth(),
          targetNode->getGridHeight(), targetNode.get());
    }

    // We ALWAYS move the floating physical coordinate of the node to follow the
    // mouse linearly. This must be outside the if block so it updates
    // continuously!
    targetNode->nodeX = dragStartGridX * 100.0f + deltaX;
    targetNode->nodeY = dragStartGridY * 100.0f + deltaY;

    if (onPositionChanged)
      onPositionChanged();
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
  if (isDraggingNode) {
    // Snap to the ghost slot if it's valid...
    if (parentCanvas.isGhostValid()) {
      targetNode->gridX = parentCanvas.getGhostX();
      targetNode->gridY = parentCanvas.getGhostY();
    } else {
      // Otherwise, instead of just popping back to start, attempt to find the
      // nearest valid spiral
      auto nearest = parentCanvas.getEngine().findClosestFreeSpot(
          parentCanvas.getGhostX(), parentCanvas.getGhostY(),
          targetNode->getGridWidth(), targetNode->getGridHeight(),
          targetNode.get());
      targetNode->gridX = nearest.x;
      targetNode->gridY = nearest.y;
    }

    // Lock the physical layout to the final grid slot
    targetNode->nodeX = (float)(targetNode->gridX * 100);
    targetNode->nodeY = (float)(targetNode->gridY * 100);

    parentCanvas.clearGhostTarget();

    if (onPositionChanged)
      onPositionChanged();
  }

  isDraggingCable = false;
  isDraggingNode = false;
}

juce::Rectangle<int> NodeBlock::getInputPortRect(int portIndex) const {
  int y = HEADER_HEIGHT + 10 + portIndex * PORT_SPACING;
  // Flush with the left edge. Width is RADIUS. Height is 2*RADIUS.
  return juce::Rectangle<int>(0, y - PORT_RADIUS, PORT_RADIUS, PORT_RADIUS * 2);
}

juce::Rectangle<int> NodeBlock::getOutputPortRect(int portIndex) const {
  int y = HEADER_HEIGHT + 10 + portIndex * PORT_SPACING;
  // Flush with the right edge. Width is RADIUS. Height is 2*RADIUS.
  return juce::Rectangle<int>(getWidth() - PORT_RADIUS, y - PORT_RADIUS,
                              PORT_RADIUS, PORT_RADIUS * 2);
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
