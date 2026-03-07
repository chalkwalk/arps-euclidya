#include "GraphCanvas.h"

GraphCanvas::GraphCanvas(GraphEngine &engine,
                         juce::AudioProcessorValueTreeState &apvtsRef,
                         juce::CriticalSection &lock)
    : graphEngine(engine), apvts(apvtsRef), graphLock(lock) {
  setWantsKeyboardFocus(true);

  addAndMakeVisible(hScroll);
  addAndMakeVisible(vScroll);
  hScroll.addListener(this);
  vScroll.addListener(this);
  addAndMakeVisible(zoomFitButton);
  zoomFitButton.setButtonText("[ ]");
  zoomFitButton.setTooltip("Zoom to Fit");
  zoomFitButton.onClick = [this] { zoomToFit(); };

  hScroll.setColour(juce::ScrollBar::trackColourId, juce::Colour(0x66111111));
  hScroll.setColour(juce::ScrollBar::thumbColourId, juce::Colour(0xff555555));
  vScroll.setColour(juce::ScrollBar::trackColourId, juce::Colour(0x66111111));
  vScroll.setColour(juce::ScrollBar::thumbColourId, juce::Colour(0xff555555));

  hScroll.setAutoHide(false);
  vScroll.setAutoHide(false);

  // Intercept all child mouse events to support universal middle-click panning
  addMouseListener(this, true);
}

void GraphCanvas::rebuild() {
  nodeBlocks.clear();

  const juce::ScopedLock sl(graphLock);
  auto &nodes = graphEngine.getNodes();

  for (auto &node : nodes) {
    auto *block = new NodeBlock(node, apvts, *this);
    // The bounds are set via updateTransforms later
    // block->setTopLeftPosition((int)node->nodeX, (int)node->nodeY);

    block->onDelete = [this, nodePtr = node.get()]() {
      const juce::ScopedLock sl2(graphLock);
      graphEngine.removeNode(nodePtr);
      rebuild();
      if (onGraphChanged)
        onGraphChanged();
    };

    block->onPositionChanged = [this]() {
      updateTransforms();
      repaint();
    };

    // Selection: bring to front when clicked anywhere on the block
    block->onSelected = [this, b = block]() {
      selectNode(b->getNode().get());
      b->toFront(true);
      hScroll.toFront(false);
      vScroll.toFront(false);
      zoomFitButton.toFront(false);
      repaint();
    };

    // Wire port dragging callbacks
    block->onPortDragStart = [this](NodeBlock *b, int port, bool isOutput,
                                    juce::Point<int> canvasPos) {
      startCableDrag(b, port, isOutput, canvasPos);
    };
    block->onPortDragging = [this](juce::Point<int> canvasPos) {
      updateCableDrag(canvasPos);
    };
    block->onPortDragEnd = [this](juce::Point<int> canvasPos) {
      endCableDrag(canvasPos);
    };

    addAndMakeVisible(block);
    nodeBlocks.add(block);
  }

  hScroll.toFront(false);
  vScroll.toFront(false);
  zoomFitButton.toFront(false);

  updateTransforms();
  repaint();
}

juce::AffineTransform GraphCanvas::getCameraTransform() const {
  return juce::AffineTransform::scale(zoomFactor).translated(panX, panY);
}

void GraphCanvas::updateTransforms() {
  auto transform = getCameraTransform();
  for (auto *block : nodeBlocks) {
    block->setTransform(transform);
    block->setBounds((int)block->getNode()->nodeX, (int)block->getNode()->nodeY,
                     block->getWidth(), block->getHeight());
  }
  updateScrollBars();
}

void GraphCanvas::addNodeAtDefaultPosition(std::shared_ptr<GraphNode> node) {
  float defaultX = 50.0f;
  float defaultY = 50.0f;

  if (!nodeBlocks.isEmpty()) {
    auto *lastBlock = nodeBlocks.getLast();
    defaultX = lastBlock->getNode()->nodeX;
    defaultY = lastBlock->getNode()->nodeY + lastBlock->getHeight() + 30;
  }

  node->nodeX = defaultX;
  node->nodeY = defaultY;

  {
    const juce::ScopedLock sl(graphLock);
    graphEngine.addNode(node);
  }

  rebuild();
}

void GraphCanvas::paint(juce::Graphics &g) {
  // Background
  g.fillAll(juce::Colour(0xff1a1a1a));

  // Draw infinitely panning grid dots
  g.setColour(juce::Colour(0xff2a2a2a));

  float scaledGrid = (float)GraphCanvas::GRID_PITCH * zoomFactor;
  float scaledInner5 = 5.0f * zoomFactor;
  float scaledInner95 = 95.0f * zoomFactor;

  float offsetX = std::fmod(panX, scaledGrid);
  if (offsetX > 0)
    offsetX -= scaledGrid;
  float offsetY = std::fmod(panY, scaledGrid);
  if (offsetY > 0)
    offsetY -= scaledGrid;

  for (float x = offsetX; x < getWidth(); x += scaledGrid) {
     g.fillRect(x + scaledInner5, 0.0f, 1.0f, (float)getHeight());
     g.fillRect(x + scaledInner95, 0.0f, 1.0f, (float)getHeight());
   }
   for (float y = offsetY; y < getHeight(); y += scaledGrid) {
     g.fillRect(0.0f, y + scaledInner5, (float)getWidth(), 1.0f);
     g.fillRect(0.0f, y + scaledInner95, (float)getWidth(), 1.0f);
  }

  // Warning banner for large sequences
  if (hasLargeSequenceWarning) {
    auto bannerArea = getLocalBounds().removeFromTop(28);
    g.setColour(juce::Colour(0xcc8B4513)); // Dark orange background
    g.fillRect(bannerArea);
    g.setColour(juce::Colours::white);
    g.setFont(12.0f);
    g.drawText(
        juce::CharPointer_UTF8(
            "\xe2\x9a\xa0 A sequence exceeds 10,000 steps. This may cycle "
            "over hundreds of bars and is likely unintentional."),
        bannerArea.reduced(8, 0), juce::Justification::centredLeft);
  }
}

void GraphCanvas::paintOverChildren(juce::Graphics &g) {
  g.saveState();

  if (hScroll.isVisible())
    g.excludeClipRegion(hScroll.getBounds());
  if (vScroll.isVisible())
    g.excludeClipRegion(vScroll.getBounds());
  if (zoomFitButton.isVisible())
    g.excludeClipRegion(zoomFitButton.getBounds());

  g.addTransform(getCameraTransform());

  // Draw all existing cables ON TOP of nodes
  const juce::ScopedLock sl(graphLock);
  auto &nodes = graphEngine.getNodes();

  for (auto &node : nodes) {
    auto *sourceBlock = findBlockForNode(node.get());
    if (sourceBlock == nullptr)
      continue;

    for (const auto &[outPort, connVec] : node->getConnections()) {
      for (const auto &conn : connVec) {
        auto *targetBlock = findBlockForNode(conn.targetNode);
        if (targetBlock == nullptr)
          continue;

        auto start = sourceBlock->getOutputPortCentre(outPort);
        auto end = targetBlock->getInputPortCentre(conn.targetInputPort);

        // Determine cable highlight: bright if connected to selected node, dim
        // otherwise
        bool isSelected =
            (selectedNode != nullptr) &&
            (node.get() == selectedNode || conn.targetNode == selectedNode);

        // Check if this cable carries a large sequence
        bool isLarge = false;
        auto &outSeq = node->getOutputSequence(outPort);
        if (outSeq.size() > 10000)
          isLarge = true;

        drawCable(g, start, end, false, isLarge, isSelected);
      }
    }
  }

  // Draw the in-progress cable drag
  if (isDraggingCable && cableDragSourceBlock != nullptr) {
    juce::Point<int> start;
    if (cableDragFromOutput) {
      start = cableDragSourceBlock->getOutputPortCentre(cableDragSourcePort);
    } else {
      start = cableDragSourceBlock->getInputPortCentre(cableDragSourcePort);
    }
    float ex = (float)cableDragEnd.x;
    float ey = (float)cableDragEnd.y;
    getCameraTransform().inverted().transformPoint(ex, ey);
    drawCable(g, start,
              juce::Point<int>(juce::roundToInt(ex), juce::roundToInt(ey)),
              true);
  }

  g.restoreState(); // Pop the camera transform to draw tooltips in screen space

  // Draw cable hover tooltip
  if (showCableTooltip) {
    auto font = juce::Font(juce::FontOptions(12.0f));
    int textW = font.getStringWidth(cableTooltipText) + 12;
    int textH = 20;
    auto tooltipRect = juce::Rectangle<int>(
        cableTooltipPos.x + 10, cableTooltipPos.y - 25, textW, textH);

    g.setColour(juce::Colour(0xee222222));
    g.fillRoundedRectangle(tooltipRect.toFloat(), 4.0f);
    g.setColour(juce::Colour(0xff888888));
    g.drawRoundedRectangle(tooltipRect.toFloat(), 4.0f, 1.0f);
    g.setColour(juce::Colours::white);
    g.setFont(font);
    g.drawText(cableTooltipText, tooltipRect, juce::Justification::centred);
  }
}

void GraphCanvas::drawCable(juce::Graphics &g, juce::Point<int> start,
                            juce::Point<int> end, bool highlighted,
                            bool warning, bool selected) {
  juce::Path path;
  path.startNewSubPath(start.toFloat());

  float dx = std::max(std::abs((float)(end.x - start.x)) * 0.5f, 40.0f);
  path.cubicTo(start.x + dx, (float)start.y, end.x - dx, (float)end.y,
               (float)end.x, (float)end.y);

  if (highlighted) {
    g.setColour(juce::Colour(0xffeeee44));
    g.strokePath(path, juce::PathStrokeType(3.5f));
  } else if (warning) {
    g.setColour(juce::Colour(0xffff6633));
    g.strokePath(path, juce::PathStrokeType(3.0f));
  } else if (selected) {
    g.setColour(juce::Colour(0xffdddddd)); // Bright white
    g.strokePath(path, juce::PathStrokeType(3.0f));
  } else {
    // Dimmed cable when a node is selected but this cable isn't connected to it
    juce::Colour dimColor = (selectedNode != nullptr)
                                ? juce::Colour(0xff555555)
                                : juce::Colour(0xffdddddd);
    g.setColour(dimColor);
    g.strokePath(path, juce::PathStrokeType(2.0f));
  }
}

void GraphCanvas::mouseDown(const juce::MouseEvent &e) {
  if (e.eventComponent != this && !e.mods.isMiddleButtonDown())
    return;

  if (e.mods.isRightButtonDown()) {
    float mx = (float)e.getPosition().x;
    float my = (float)e.getPosition().y;
    getCameraTransform().inverted().transformPoint(mx, my);
    auto localPos = juce::Point<float>(mx, my);
    const juce::ScopedLock sl(graphLock);
    auto &nodes = graphEngine.getNodes();

    for (auto &node : nodes) {
      auto *sourceBlock = findBlockForNode(node.get());
      if (sourceBlock == nullptr)
        continue;

      for (const auto &[outPort, connVec] : node->getConnections()) {
        for (const auto &conn : connVec) {
          auto *targetBlock = findBlockForNode(conn.targetNode);
          if (targetBlock == nullptr)
            continue;

          auto start = sourceBlock->getOutputPortCentre(outPort);
          auto end = targetBlock->getInputPortCentre(conn.targetInputPort);

          // Check proximity to cable using several sample points
          juce::Path path;
          path.startNewSubPath(start.toFloat());
          float dx = std::max(std::abs((float)(end.x - start.x)) * 0.5f, 40.0f);
          path.cubicTo(start.x + dx, (float)start.y, end.x - dx, (float)end.y,
                       (float)end.x, (float)end.y);

          juce::Point<float> nearest;
          path.getNearestPoint(localPos, nearest);
          if (nearest.getDistanceFrom(localPos) < 12.0f) {
            graphEngine.removeConnection(node.get(), outPort, conn.targetNode,
                                         conn.targetInputPort);
            repaint();
            return;
          }
        }
      }
    }
  } else if (e.mods.isMiddleButtonDown() ||
             (!e.mods.isPopupMenu() && !e.mods.isAnyModifierKeyDown())) {
    // If we click empty background, initiate pan
    isPanning = true;
    lastPanScreenPos = e.getScreenPosition();
  }
}

void GraphCanvas::mouseDrag(const juce::MouseEvent &e) {
  if (e.eventComponent != this && !e.mods.isMiddleButtonDown())
    return;

  if (isPanning) {
    auto delta = e.getScreenPosition() - lastPanScreenPos;
    panX += (float)delta.x;
    panY += (float)delta.y;
    lastPanScreenPos = e.getScreenPosition();
    updateTransforms();
    repaint();
  }
}

void GraphCanvas::mouseUp(const juce::MouseEvent &e) {
  juce::ignoreUnused(e);
  if (e.eventComponent != this && !e.mods.isMiddleButtonDown())
    return;

  if (isPanning) {
    isPanning = false;
  }
}

void GraphCanvas::mouseWheelMove(const juce::MouseEvent &e,
                                 const juce::MouseWheelDetails &wheel) {
  if (e.eventComponent != this)
    return;

  // Semantic zoom natively mapping focal origin
  float wheelAmount =
      std::abs(wheel.deltaY) > 0.0001f ? wheel.deltaY : wheel.deltaX;
  if (std::abs(wheelAmount) < 0.0001f)
    return;

  float zoomDelta = wheelAmount > 0.0f ? 1.1f : 0.9f;
  auto mousePos = e.position;

  float newZoom = juce::jlimit(0.2f, 3.0f, zoomFactor * zoomDelta);

  if (std::abs(newZoom - zoomFactor) > 0.0001f) {
    float worldX = (mousePos.x - panX) / zoomFactor;
    float worldY = (mousePos.y - panY) / zoomFactor;

    zoomFactor = newZoom;
    panX = mousePos.x - worldX * zoomFactor;
    panY = mousePos.y - worldY * zoomFactor;

    updateTransforms();
    repaint();
  }
}

void GraphCanvas::startCableDrag(NodeBlock *block, int portIndex, bool isOutput,
                                 juce::Point<int> canvasPos) {
  isDraggingCable = true;
  cableDragSourceBlock = block;
  cableDragSourcePort = portIndex;
  cableDragFromOutput = isOutput;
  cableDragEnd = canvasPos; // This is in screen coordinates
  repaint();
}

void GraphCanvas::updateCableDrag(juce::Point<int> canvasPos) {
  if (isDraggingCable) {
    cableDragEnd = canvasPos; // This is in screen coordinates
    repaint();
  }
}

void GraphCanvas::endCableDrag(juce::Point<int> canvasPos) {
  if (!isDraggingCable)
    return;

  isDraggingCable = false;

  // Find target port under the release point
  for (auto *block : nodeBlocks) {
    // Convert canvasPos (screen coords) to block's local world coords for
    // hit-testing
    float cx = (float)canvasPos.x;
    float cy = (float)canvasPos.y;
    getCameraTransform().inverted().transformPoint(cx, cy);
    auto blockLocalWorld =
        juce::Point<float>(cx, cy) - block->getPosition().toFloat();

    if (cableDragFromOutput) {
      // Dragging from output → looking for an input port
      int numIn = block->getNode()->getNumInputPorts();
      for (int i = 0; i < numIn; ++i) {
        auto portCentreWorld = block->getInputPortCentre(i).toFloat() -
                               block->getPosition().toFloat();
        if (blockLocalWorld.getDistanceFrom(portCentreWorld) <=
            NodeBlock::PORT_HIT_RADIUS) {
          const juce::ScopedLock sl(graphLock);
          graphEngine.addExplicitConnection(
              cableDragSourceBlock->getNode().get(), cableDragSourcePort,
              block->getNode().get(), i);
          break;
        }
      }
    } else {
      // Dragging from input → looking for an output port
      int numOut = block->getNode()->getNumOutputPorts();
      for (int i = 0; i < numOut; ++i) {
        auto portCentreWorld = block->getOutputPortCentre(i).toFloat() -
                               block->getPosition().toFloat();
        if (blockLocalWorld.getDistanceFrom(portCentreWorld) <=
            NodeBlock::PORT_HIT_RADIUS) {
          const juce::ScopedLock sl(graphLock);
          graphEngine.addExplicitConnection(
              block->getNode().get(), i, cableDragSourceBlock->getNode().get(),
              cableDragSourcePort);
          break;
        }
      }
    }
  }

  cableDragSourceBlock = nullptr;
  cableDragSourcePort = -1;
  repaint();
}

NodeBlock *GraphCanvas::findBlockForNode(GraphNode *node) const {
  for (auto *block : nodeBlocks) {
    if (block->getNode().get() == node)
      return block;
  }
  return nullptr;
}

void GraphCanvas::resized() { updateScrollBars(); }

void GraphCanvas::scrollBarMoved(juce::ScrollBar *scrollBar,
                                 double newRangeStart) {
  if (scrollBar == &hScroll) {
    panX = (float)(-newRangeStart * zoomFactor);
  } else if (scrollBar == &vScroll) {
    panY = (float)(-newRangeStart * zoomFactor);
  }
  updateTransforms();
  repaint();
}

void GraphCanvas::updateScrollBars() {
  if (getWidth() <= 0 || getHeight() <= 0)
    return;

  float minX = 0.0f, minY = 0.0f, maxX = 0.0f, maxY = 0.0f;
  bool hasNodes = false;

  for (auto *block : nodeBlocks) {
    float x = block->getNode()->nodeX;
    float y = block->getNode()->nodeY;
    float w = (float)block->getWidth();
    float h = (float)block->getHeight();
    if (!hasNodes) {
      minX = x;
      minY = y;
      maxX = x + w;
      maxY = y + h;
      hasNodes = true;
    } else {
      minX = std::min(minX, x);
      minY = std::min(minY, y);
      maxX = std::max(maxX, x + w);
      maxY = std::max(maxY, y + h);
    }
  }

  if (hasNodes) {
    minX -= 60.0f;
    minY -= 60.0f;
    maxX += 60.0f;
    maxY += 60.0f;
  } else {
    minX = 0.0f;
    minY = 0.0f;
    maxX = 100.0f;
    maxY = 100.0f;
  }

  float currentViewX = -panX / zoomFactor;
  float currentViewY = -panY / zoomFactor;
  float viewW = (float)getWidth() / zoomFactor;
  float viewH = (float)getHeight() / zoomFactor;

  // Union of node bounding box AND current viewport position
  float limitMinX = std::min(minX, currentViewX);
  float limitMaxX = std::max(maxX, currentViewX + viewW);
  float limitMinY = std::min(minY, currentViewY);
  float limitMaxY = std::max(maxY, currentViewY + viewH);

  bool showH = (limitMaxX - limitMinX) > viewW + 1.0f;
  bool showV = (limitMaxY - limitMinY) > viewH + 1.0f;

  hScroll.setVisible(showH);
  vScroll.setVisible(showV);
  zoomFitButton.setVisible(showH || showV);

  int sbW = 14;
  int w = getWidth();
  int h = getHeight();

  int hScrollWidth = showV ? w - sbW : w;
  int vScrollHeight = showH ? h - sbW : h;

  hScroll.setBounds(0, h - sbW, hScrollWidth, sbW);
  vScroll.setBounds(w - sbW, 0, sbW, vScrollHeight);
  zoomFitButton.setBounds(w - sbW, h - sbW, sbW, sbW);

  hScroll.setRangeLimits(limitMinX, limitMaxX);
  hScroll.setCurrentRange(currentViewX, viewW, juce::dontSendNotification);

  vScroll.setRangeLimits(limitMinY, limitMaxY);
  vScroll.setCurrentRange(currentViewY, viewH, juce::dontSendNotification);
}

void GraphCanvas::zoomToFit() {
  if (nodeBlocks.isEmpty())
    return;

  float minX = nodeBlocks[0]->getNode()->nodeX;
  float minY = nodeBlocks[0]->getNode()->nodeY;
  float maxX = minX + nodeBlocks[0]->getWidth();
  float maxY = minY + nodeBlocks[0]->getHeight();

  for (auto *block : nodeBlocks) {
    float x = block->getNode()->nodeX;
    float y = block->getNode()->nodeY;
    float w = (float)block->getWidth();
    float h = (float)block->getHeight();
    minX = std::min(minX, x);
    minY = std::min(minY, y);
    maxX = std::max(maxX, x + w);
    maxY = std::max(maxY, y + h);
  }

  float pad = 60.0f;
  minX -= pad;
  minY -= pad;
  maxX += pad;
  maxY += pad;

  float worldW = maxX - minX;
  float worldH = maxY - minY;

  if (worldW <= 0.0f || worldH <= 0.0f)
    return;

  float zoomX = (float)getWidth() / worldW;
  float zoomY = (float)getHeight() / worldH;

  zoomFactor = juce::jlimit(0.1f, 3.0f, std::min(zoomX, zoomY));

  float scaledW = worldW * zoomFactor;
  float scaledH = worldH * zoomFactor;

  panX = (getWidth() - scaledW) * 0.5f - minX * zoomFactor;
  panY = (getHeight() - scaledH) * 0.5f - minY * zoomFactor;

  updateTransforms();
  repaint();
}

void GraphCanvas::mouseMove(const juce::MouseEvent &e) {
  float mx = (float)e.getPosition().x;
  float my = (float)e.getPosition().y;
  getCameraTransform().inverted().transformPoint(mx, my);
  auto localPosWorld = juce::Point<float>(mx, my);
  bool found = false;

  const juce::ScopedLock sl(graphLock);
  auto &nodes = graphEngine.getNodes();

  for (auto &node : nodes) {
    auto *sourceBlock = findBlockForNode(node.get());
    if (sourceBlock == nullptr)
      continue;

    for (const auto &[outPort, connVec] : node->getConnections()) {
      for (const auto &conn : connVec) {
        auto *targetBlock = findBlockForNode(conn.targetNode);
        if (targetBlock == nullptr)
          continue;

        auto start = sourceBlock->getOutputPortCentre(outPort);
        auto end = targetBlock->getInputPortCentre(conn.targetInputPort);

        juce::Path path;
        path.startNewSubPath(start.toFloat());
        float dx = std::max(std::abs((float)(end.x - start.x)) * 0.5f, 40.0f);
        path.cubicTo(start.x + dx, (float)start.y, end.x - dx, (float)end.y,
                     (float)end.x, (float)end.y);

        juce::Point<float> nearest;
        path.getNearestPoint(localPosWorld, nearest);
        if (nearest.getDistanceFrom(localPosWorld) < 12.0f) {
          auto &outSeq = node->getOutputSequence(outPort);
          int stepCount = (int)outSeq.size();
          cableTooltipText = juce::String(stepCount) + " steps";
          if (stepCount > 10000)
            cableTooltipText += juce::String::fromUTF8(" \xe2\x9a\xa0");
          // position tooltip at actual cursor screen coordinate
          cableTooltipPos = e.getPosition();
          showCableTooltip = true;
          found = true;
          break;
        }
      }
      if (found)
        break;
    }
    if (found)
      break;
  }

  if (!found && showCableTooltip) {
    showCableTooltip = false;
  }

  repaint();
}

void GraphCanvas::mouseExit(const juce::MouseEvent &) {
  if (showCableTooltip) {
    showCableTooltip = false;
    repaint();
  }
}

void GraphCanvas::checkForLargeSequences() {
  bool foundLarge = false;
  const juce::ScopedLock sl(graphLock);
  auto &nodes = graphEngine.getNodes();

  for (auto &node : nodes) {
    for (const auto &[outPort, connVec] : node->getConnections()) {
      auto &outSeq = node->getOutputSequence(outPort);
      if (outSeq.size() > 10000) {
        foundLarge = true;
        break;
      }
    }
    if (foundLarge)
      break;
  }

  if (foundLarge != hasLargeSequenceWarning) {
    hasLargeSequenceWarning = foundLarge;
    repaint();
  }
}

void GraphCanvas::addNodeAtPosition(std::shared_ptr<GraphNode> node,
                                    juce::Point<int> screenPos) {
  juce::ScopedLock l(graphLock);

  // Convert screen drop pixel to internal world position considering pan & zoom
  float worldX = (float)screenPos.x;
  float worldY = (float)screenPos.y;
  getCameraTransform().inverted().transformPoint(worldX, worldY);

  node->nodeX = worldX;
  node->nodeY = worldY;

  graphEngine.addNode(node);

  // Rebuild the NodeBlock wrappers
  rebuild();

  // Recalculate and apply scaled block bounds cleanly derived from nodeX/nodeY
  updateTransforms();

  if (onGraphChanged)
    onGraphChanged();
}

bool GraphCanvas::isInterestedInDragSource(
    const SourceDetails &dragSourceDetails) {
  return dragSourceDetails.description.isString();
}

void GraphCanvas::itemDropped(const SourceDetails &dragSourceDetails) {
  if (onNodeDropped) {
    onNodeDropped(dragSourceDetails.description.toString(),
                  dragSourceDetails.localPosition);
  }
}

void GraphCanvas::selectNode(GraphNode *node) {
  if (selectedNode != node) {
    selectedNode = node;
    repaint();
  }
}
