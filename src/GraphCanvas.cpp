#include "GraphCanvas.h"

#include "ArpsLookAndFeel.h"
#include "LayoutConstants.h"
#include "MacroColours.h"
#include "NodeFactory.h"

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

  // Intercept child mouse events to support universal middle-click panning
  // We no longer add 'this' as a listener to itself (which was causing double
  // clicks) instead we add it specifically to each NodeBlock in rebuild().
  addKeyListener(this);
}

void GraphCanvas::undo() {
  if (onUndo) {
    onUndo();
  }
}

void GraphCanvas::redo() {
  if (onRedo) {
    onRedo();
  }
}

void GraphCanvas::rebuild() {
  nodeBlocks.clear();

  const juce::ScopedLock sl(graphLock);
  const auto &nodes = graphEngine.getNodes();

  for (const auto &node : nodes) {
    auto *block = new NodeBlock(node, apvts, *this);
    block->setSelectedMacroPtr(selectedMacroPtr);
    // The bounds are set via updateTransforms later
    // block->setTopLeftPosition((int)node->nodeX, (int)node->nodeY);

    block->onDelete = [this, nodePtr = node.get()]() {
      const juce::ScopedLock sl2(graphLock);
      if (performMutation) {
        performMutation([this, nodePtr]() { graphEngine.removeNode(nodePtr); });
      } else {
        graphEngine.removeNode(nodePtr);
      }
      rebuild();
      if (onGraphChanged) {
        onGraphChanged();
      }
    };

    block->onPositionChanged = [this]() {
      updateTransforms();
      refreshCableCache();
      repaint();
    };

    // Selection: bring to front when clicked anywhere on the block
    block->onSelected = [this, b = block]() {
      selectNode(b->getNode().get());
      b->toFront(false);
      hScroll.toFront(false);
      vScroll.toFront(false);
      zoomFitButton.toFront(false);
      refreshCableCache();
      repaint();
    };

    block->onAddToSelection = [this, b = block]() {
      addToSelection(b->getNode().get());
      b->toFront(false);
      hScroll.toFront(false);
      vScroll.toFront(false);
      zoomFitButton.toFront(false);
    };

    block->onHoverMacros = onHoverMacros;

    block->onReplaceRequest = [this, n = node.get()](const juce::String &type) {
      if (onNodeReplaceRequest) {
        onNodeReplaceRequest(n, type);
      }
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
    block->addMouseListener(this, false);
    nodeBlocks.add(block);
  }

  hScroll.toFront(false);
  vScroll.toFront(false);
  zoomFitButton.toFront(false);

  updateTransforms();
  refreshCableCache();
  repaint();
}

juce::AffineTransform GraphCanvas::getCameraTransform() const {
  return juce::AffineTransform::scale(zoomFactor).translated(panX, panY);
}

juce::Point<int> GraphCanvas::getViewportGridCenter() const {
  auto center = getLocalBounds().getCentre().toFloat();
  float cx = center.x;
  float cy = center.y;
  getCameraTransform().inverted().transformPoint(cx, cy);
  return {(int)std::round(cx / Layout::GridPitchFloat),
          (int)std::round(cy / Layout::GridPitchFloat)};
}

void GraphCanvas::updateTransforms() {
  auto transform = getCameraTransform();
  for (auto *block : nodeBlocks) {
    block->setTransform(transform);

    // Safety check for NaN coordinates before setting bounds
    float nx = block->getNode()->nodeX;
    float ny = block->getNode()->nodeY;
    if (std::isnan(nx) || std::isinf(nx))
      nx = 0.0f;
    if (std::isnan(ny) || std::isinf(ny))
      ny = 0.0f;

    // When a block is unfolded, its component is (gridW+2)×(gridH+2) in size
    // and its origin shifts one grid pitch above/left the logical position.
    int offsetX = block->isUnfolded() ? -Layout::GridPitch : 0;
    int offsetY = block->isUnfolded() ? -Layout::GridPitch : 0;
    block->setBounds((int)nx + offsetX, (int)ny + offsetY,
                     block->getWidth(), block->getHeight());
  }
  updateScrollBars();
}

void GraphCanvas::addNodeAtDefaultPosition(
    const std::shared_ptr<GraphNode> &node) {
  int startGridX = 0;
  int startGridY = 0;

  if (!nodeBlocks.isEmpty()) {
    auto *lastBlock = nodeBlocks.getLast();
    startGridX = lastBlock->getNode()->gridX;
    startGridY =
        lastBlock->getNode()->gridY + lastBlock->getNode()->getGridHeight();
  }

  juce::ScopedLock l(graphLock);
  auto nearest = graphEngine.findClosestFreeSpot(
      startGridX, startGridY, node->getGridWidth(), node->getGridHeight(),
      nullptr, getViewportGridCenter());
  node->gridX = nearest.x;
  node->gridY = nearest.y;

  node->nodeX =
      (float)(node->gridX * Layout::GridPitch) + Layout::TramlineOffset;
  node->nodeY =
      (float)(node->gridY * Layout::GridPitch) + Layout::TramlineOffset;

  graphEngine.addNode(node);

  rebuild();
}

void GraphCanvas::paint(juce::Graphics &g) {
  // Background
  g.fillAll(ArpsLookAndFeel::getBackgroundCharcoal());

  // Draw infinitely panning grid dots
  g.setColour(juce::Colour(0xff2a2a2a));

  float scaledGrid = Layout::GridPitchFloat * zoomFactor;
  float scaledInner5 = Layout::TramlineOffset * zoomFactor;
  float scaledInner95 =
      (Layout::GridPitchFloat - Layout::TramlineOffset) * zoomFactor;

  float offsetX = std::fmod(panX, scaledGrid);
  if (offsetX > 0) {
    offsetX -= scaledGrid;
  }
  float offsetY = std::fmod(panY, scaledGrid);
  if (offsetY > 0) {
    offsetY -= scaledGrid;
  }

  for (float x = offsetX; x < getWidth(); x += scaledGrid) {
    g.fillRect(x + scaledInner5, 0.0f, 1.0f, (float)getHeight());
    g.fillRect(x + scaledInner95, 0.0f, 1.0f, (float)getHeight());
  }
  for (float y = offsetY; y < getHeight(); y += scaledGrid) {
    g.fillRect(0.0f, y + scaledInner5, (float)getWidth(), 1.0f);
    g.fillRect(0.0f, y + scaledInner95, (float)getWidth(), 1.0f);
  }

  // Draw node insertion "ghost" target
  if (showGhostTarget) {
    g.addTransform(getCameraTransform());

    // 1. Calculate world-space rectangles
    auto getRect = [](int gx, int gy, int gw, int gh) {
      float x = (float)(gx * Layout::GridPitch) + Layout::TramlineOffset;
      float y = (float)(gy * Layout::GridPitch) + Layout::TramlineOffset;
      float w = (float)(gw * Layout::GridPitch) - Layout::TramlineMargin;
      float h = (float)(gh * Layout::GridPitch) - Layout::TramlineMargin;
      return juce::Rectangle<float>(x, y, w, h);
    };

    auto mouseRect =
        getRect(ghostTargetX, ghostTargetY, ghostTargetW, ghostTargetH);
    auto resolvedRect =
        getRect(ghostResolvedX, ghostResolvedY, ghostTargetW, ghostTargetH);

    // 2. Draw "Flight Path" if the target is redirected
    if (!ghostIsValid && hasGhostResolved) {
      juce::Path flightPath;
      flightPath.startNewSubPath(mouseRect.getCentre());
      flightPath.lineTo(resolvedRect.getCentre());

      // Ribbon Glow
      g.setColour(juce::Colour(0x440df0e3));  // Neon Cyan glow
      g.strokePath(flightPath, juce::PathStrokeType(6.0f));

      // Dashed Line
      juce::Path dashedPath;
      float dashLengths[] = {8.0f, 4.0f};
      juce::PathStrokeType(2.0f).createDashedStroke(dashedPath, flightPath,
                                                    dashLengths, 2);
      g.setColour(juce::Colour(0xaa0df0e3));
      g.fillPath(dashedPath);
    }

    // 3. Draw Mouse Ghost (Dimmed / Indicator)
    if (ghostIsValid) {
      g.setColour(juce::Colour(0x4444ff44));  // translucent green
      g.fillRoundedRectangle(mouseRect, 6.0f);
    } else {
      g.setColour(juce::Colour(0x22ff4444));  // faint red
      g.fillRoundedRectangle(mouseRect, 6.0f);
      g.setColour(juce::Colour(0x44ff4444));
      g.drawRoundedRectangle(mouseRect, 6.0f, 1.0f);
    }

    // 4. Draw Resolved Ghost (The actual destination)
    if (!ghostIsValid && hasGhostResolved) {
      g.setColour(juce::Colour(0x6644ff44));  // stronger green
      g.fillRoundedRectangle(resolvedRect, 6.0f);
      g.setColour(juce::Colour(0xaa44ff44));
      g.drawRoundedRectangle(resolvedRect, 6.0f, 2.0f);
    }

    g.addTransform(getCameraTransform().inverted());
  }

  // Warning banner for large sequences
  if (hasLargeSequenceWarning) {
    auto bannerArea = getLocalBounds().removeFromTop(28);
    g.setColour(juce::Colour(0xcc8B4513));  // Dark orange background
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

  if (hScroll.isVisible()) {
    g.excludeClipRegion(hScroll.getBounds());
  }
  if (vScroll.isVisible()) {
    g.excludeClipRegion(vScroll.getBounds());
  }
  if (zoomFitButton.isVisible()) {
    g.excludeClipRegion(zoomFitButton.getBounds());
  }

  g.addTransform(getCameraTransform());

  // 1. Draw background cables
  for (const auto &cable : cachedCables) {
    if (!cable.isSelected && !proximityCableID.matches(cable)) {
      drawCable(g, cable.path, false, cable.isLarge, false, cable.portType);
    }
  }

  // 2. Draw foreground/selected cables
  for (const auto &cable : cachedCables) {
    if (cable.isSelected && !proximityCableID.matches(cable)) {
      drawCable(g, cable.path, false, cable.isLarge, true, cable.portType);
    }
  }

  // 3. Draw proximity highlight cable (on top)
  if (proximityCableID.isValid()) {
    for (const auto &cable : cachedCables) {
      if (proximityCableID.matches(cable)) {
        drawCable(g, cable.path, true, cable.isLarge, true, cable.portType);
        break;
      }
    }
  }

  // 3. Third Pass: Draw the in-progress cable drag
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

    juce::Path dragPath;
    dragPath.startNewSubPath(start.toFloat());
    juce::Point<int> end(juce::roundToInt(ex), juce::roundToInt(ey));
    float dx = std::max(std::abs((float)(end.x - start.x)) * 0.5f, 40.0f);
    dragPath.cubicTo(start.x + dx, (float)start.y, end.x - dx, (float)end.y,
                     (float)end.x, (float)end.y);

    drawCable(g, dragPath, true, false, true, GraphNode::PortType::Notes);
  }

  g.restoreState();  // Pop the camera transform to draw tooltips in screen
                     // space

  // Reject flash — red glow at the drop point when a type mismatch is detected
  if (rejectFlashAlpha > 0.01f) {
    const float r = 22.0f;
    g.setColour(juce::Colours::red.withAlpha(rejectFlashAlpha * 0.25f));
    g.fillEllipse(rejectFlashPos.x - r, rejectFlashPos.y - r, r * 2.0f,
                  r * 2.0f);
    g.setColour(juce::Colours::red.withAlpha(rejectFlashAlpha * 0.85f));
    g.drawEllipse(rejectFlashPos.x - r, rejectFlashPos.y - r, r * 2.0f,
                  r * 2.0f, 2.5f);
  }

  // Draw rubber-band selection box (screen-space, after camera pop)
  if (isBoxSelecting) {
    auto boxRect = juce::Rectangle<float>::leftTopRightBottom(
        std::min(boxSelectStartScreen.x, boxSelectCurrentScreen.x),
        std::min(boxSelectStartScreen.y, boxSelectCurrentScreen.y),
        std::max(boxSelectStartScreen.x, boxSelectCurrentScreen.x),
        std::max(boxSelectStartScreen.y, boxSelectCurrentScreen.y));
    g.setColour(juce::Colour(0xff00ffff).withAlpha(0.06f));
    g.fillRect(boxRect);
    g.setColour(juce::Colour(0xff00ffff).withAlpha(0.5f));
    g.drawRect(boxRect, 1.0f);
  }

  // Draw a macro-colored border when a macro is selected
  if (selectedMacroPtr != nullptr && *selectedMacroPtr != -1) {
    auto c = getMacroColour(*selectedMacroPtr);
    g.setColour(c.withAlpha(0.6f));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.5f), 4.0f,
                           3.0f);
  }

  // Draw cable hover tooltip
  if (showCableTooltip) {
    auto font = juce::Font(juce::FontOptions(12.0f));
    int textW =
        juce::GlyphArrangement::getStringWidthInt(font, cableTooltipText) + 12;
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

void GraphCanvas::setSelectedMacroPtr(int *ptr) {
  selectedMacroPtr = ptr;
  for (auto *block : nodeBlocks)
    block->setSelectedMacroPtr(ptr);
  repaint();
}

void GraphCanvas::setHighlightedMacro(int macroIndex) {
  for (auto *block : nodeBlocks)
    block->setHighlightedMacro(macroIndex);
}

void GraphCanvas::refreshCableCache() {
  cachedCables.clear();
  const juce::ScopedLock sl(graphLock);
  const auto &nodes = graphEngine.getNodes();

  for (const auto &node : nodes) {
    auto *sourceBlock = findBlockForNode(node.get());
    if (sourceBlock == nullptr) {
      continue;
    }

    for (const auto &[outPort, connVec] : node->getConnections()) {
      for (const auto &conn : connVec) {
        auto *targetBlock = findBlockForNode(conn.targetNode);
        if (targetBlock == nullptr) {
          continue;
        }

        CachedCable cable;
        cable.sourceNode = node.get();
        cable.sourcePort = outPort;
        cable.targetNode = conn.targetNode;
        cable.targetPort = conn.targetInputPort;

        auto start = sourceBlock->getOutputPortCentre(outPort);
        auto end = targetBlock->getInputPortCentre(conn.targetInputPort);

        auto startF = start.toFloat();
        auto endF = end.toFloat();

        // Final safety check for NaN/Inf before path operations
        if (std::isnan(startF.x) || std::isinf(startF.x))
          startF.x = 0.0f;
        if (std::isnan(startF.y) || std::isinf(startF.y))
          startF.y = 0.0f;
        if (std::isnan(endF.x) || std::isinf(endF.x))
          endF.x = 0.0f;
        if (std::isnan(endF.y) || std::isinf(endF.y))
          endF.y = 0.0f;

        cable.path.startNewSubPath(startF);
        float dx = std::max(std::abs(endF.x - startF.x) * 0.5f, 40.0f);
        if (std::isnan(dx) || std::isinf(dx))
          dx = 40.0f;

        cable.path.cubicTo(startF.x + dx, startF.y, endF.x - dx, endF.y, endF.x,
                           endF.y);

        cable.portType = graphEngine.getEffectiveOutputPortType(node.get(), outPort);
        const auto &outSeq = node->getOutputSequence(outPort);
        cable.stepCount = (int)outSeq.size();
        cable.activeStepCount = 0;
        for (const auto &step : outSeq) {
          if (!step.empty()) {
            cable.activeStepCount++;
          }
        }
        cable.isLarge = (cable.stepCount > 10000);
        cable.isSelected =
            !selectedNodes.empty() &&
            (selectedNodes.count(node.get()) > 0 ||
             selectedNodes.count(conn.targetNode) > 0);

        cachedCables.push_back(std::move(cable));
      }
    }
  }
}

void GraphCanvas::drawCable(juce::Graphics &g, const juce::Path &path,
                            bool highlighted, bool warning, bool isForeground,
                            GraphNode::PortType portType) {
  // 1. Drop Shadow (Subtle dark offset)
  auto shadowPath = path;
  shadowPath.applyTransform(juce::AffineTransform::translation(1.0f, 1.5f));
  g.setColour(juce::Colours::black.withAlpha(0.4f));
  g.strokePath(shadowPath, juce::PathStrokeType(2.5f));

  // 2. Base Cable Color and Glow — colour by source port type
  juce::Colour baseColor;
  if (highlighted) {
    baseColor = juce::Colour(0xffeeee44);  // Yellow highlight
  } else if (warning) {
    baseColor = juce::Colour(0xffff6633);  // Orange (large sequence)
  } else if (portType == GraphNode::PortType::CC) {
    baseColor = juce::Colour(0xffaa44ff);  // Violet for CC
  } else if (portType == GraphNode::PortType::Notes) {
    baseColor = juce::Colour(0xffd4a017);  // Amber/gold for Notes
  } else {
    baseColor = juce::Colour(0xffaaaaaa);  // Cool grey for Agnostic
  }

  // Dimming for background (non-selected) cables
  if (!isForeground && !highlighted && !selectedNodes.empty()) {
    baseColor = baseColor.withMultipliedAlpha(0.4f);
  }

  // Multi-stroke Bloom
  if (isForeground || highlighted || (selectedNodes.empty() && !warning)) {
    // Large outer glow
    g.setColour(baseColor.withAlpha(0.15f));
    g.strokePath(path, juce::PathStrokeType(8.0f));

    // Medium glow
    g.setColour(ArpsLookAndFeel::getNeonColor().withAlpha(0.2f));
    g.strokePath(path, juce::PathStrokeType(5.0f));
  }

  // 3. Main Cable Stroke
  g.setColour(baseColor);
  float strokeThickness = (isForeground || highlighted) ? 3.0f : 2.0f;
  if (highlighted) {
    strokeThickness = 3.5f;
  }

  g.strokePath(path, juce::PathStrokeType(strokeThickness));

  // 4. Center Shine (if highlighted or foreground)
  if (isForeground || highlighted) {
    g.setColour(juce::Colours::white.withAlpha(0.5f));
    g.strokePath(path, juce::PathStrokeType(1.0f));
  }
}

void GraphCanvas::mouseDown(const juce::MouseEvent &e) {
  if (e.eventComponent != this && !e.mods.isMiddleButtonDown()) {
    return;
  }

  if (e.mods.isRightButtonDown()) {
    float mx = (float)e.getPosition().x;
    float my = (float)e.getPosition().y;
    getCameraTransform().inverted().transformPoint(mx, my);
    auto localPos = juce::Point<float>(mx, my);
    const juce::ScopedLock sl(graphLock);
    for (const auto &cable : cachedCables) {
      juce::Point<float> nearest;
      cable.path.getNearestPoint(localPos, nearest);
      if (nearest.getDistanceFrom(localPos) < 12.0f) {
        if (performMutation) {
          performMutation([this, &cable]() {
            graphEngine.removeConnection(cable.sourceNode, cable.sourcePort,
                                         cable.targetNode, cable.targetPort);
          });
        } else {
          graphEngine.removeConnection(cable.sourceNode, cable.sourcePort,
                                       cable.targetNode, cable.targetPort);
        }
        refreshCableCache();
        repaint();
        return;
      }
    }

    // If we reach here, no cable was hit by the right-click — show insertion
    // menu
    juce::PopupMenu m;
    auto types = NodeFactory::getAvailableNodeTypes();
    int i = 1;
    for (const auto &type : types) {
      m.addItem(i++, type);
    }

    m.showMenuAsync(juce::PopupMenu::Options(), [this, types, e](int result) {
      if (result > 0 && result <= (int)types.size()) {
        if (onNodeDropped) {
          onNodeDropped(types[(size_t)result - 1], e.getPosition());
        }
      }
    });
    return;
  }

  if (e.mods.isMiddleButtonDown()) {
    grabKeyboardFocus();
    isPanning = true;
    lastPanScreenPos = e.getScreenPosition();
  } else if (!e.mods.isPopupMenu() && !e.mods.isAnyModifierKeyDown() &&
             e.eventComponent == this) {
    // Left-click on empty canvas: start a rubber-band selection
    grabKeyboardFocus();
    isBoxSelecting = true;
    boxSelectStartScreen = e.position.toFloat();
    boxSelectCurrentScreen = e.position.toFloat();
  }
}

void GraphCanvas::mouseDrag(const juce::MouseEvent &e) {
  if (e.eventComponent != this && !e.mods.isMiddleButtonDown()) {
    return;
  }

  if (isPanning) {
    auto delta = e.getScreenPosition() - lastPanScreenPos;
    panX += (float)delta.x;
    panY += (float)delta.y;
    lastPanScreenPos = e.getScreenPosition();
    updateTransforms();
    repaint();
    return;
  }

  if (isBoxSelecting) {
    boxSelectCurrentScreen = e.position.toFloat();
    repaint();
  }
}

void GraphCanvas::mouseUp(const juce::MouseEvent &e) {
  if (e.eventComponent != this && !e.mods.isMiddleButtonDown()) {
    return;
  }

  if (isPanning) {
    isPanning = false;
    return;
  }

  if (isBoxSelecting) {
    isBoxSelecting = false;
    float distSq = boxSelectStartScreen.getDistanceSquaredFrom(boxSelectCurrentScreen);
    if (distSq < 16.0f) {
      // Negligible drag — treat as background click, clear selection
      clearSelection();
    } else {
      // Build a normalised screen-space rect then convert to world space
      auto screenRect = juce::Rectangle<float>::leftTopRightBottom(
          std::min(boxSelectStartScreen.x, boxSelectCurrentScreen.x),
          std::min(boxSelectStartScreen.y, boxSelectCurrentScreen.y),
          std::max(boxSelectStartScreen.x, boxSelectCurrentScreen.x),
          std::max(boxSelectStartScreen.y, boxSelectCurrentScreen.y));

      // Convert two corners to world space via inverse camera transform
      auto inv = getCameraTransform().inverted();
      float x1 = screenRect.getX(), y1 = screenRect.getY();
      float x2 = screenRect.getRight(), y2 = screenRect.getBottom();
      inv.transformPoint(x1, y1);
      inv.transformPoint(x2, y2);
      juce::Rectangle<float> worldRect(x1, y1, x2 - x1, y2 - y1);

      for (auto *block : nodeBlocks) {
        auto *node = block->getNode().get();
        juce::Rectangle<float> nodeBounds(node->nodeX, node->nodeY,
                                          (float)block->getWidth(),
                                          (float)block->getHeight());
        if (worldRect.intersects(nodeBounds)) {
          selectedNodes.insert(node);
        }
      }
      refreshCableCache();
      repaint();
    }
  }
}

void GraphCanvas::mouseWheelMove(const juce::MouseEvent &e,
                                 const juce::MouseWheelDetails &wheel) {
  if (e.eventComponent != this) {
    return;
  }

  // Semantic zoom natively mapping focal origin
  float wheelAmount =
      std::abs(wheel.deltaY) > 0.0001f ? wheel.deltaY : wheel.deltaX;
  if (std::abs(wheelAmount) < 0.0001f) {
    return;
  }

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
  cableDragEnd = canvasPos;  // This is in screen coordinates
  repaint();
}

void GraphCanvas::updateCableDrag(juce::Point<int> canvasPos) {
  if (isDraggingCable) {
    cableDragEnd = canvasPos;  // This is in screen coordinates
    repaint();
  }
}

void GraphCanvas::requestNodeClone(GraphNode *original, int gridX,
                                   int gridY) const {
  if (onNodeCloneRequest) {
    onNodeCloneRequest(original, gridX, gridY);
  }
}

void GraphCanvas::attemptProximityConnection(GraphNode *droppedNode,
                                             juce::Point<int> mousePos) {
  if (droppedNode == nullptr) {
    return;
  }

  // Transform mouse position to world coordinates
  float fx = (float)mousePos.x;
  float fy = (float)mousePos.y;
  getCameraTransform().inverted().transformPoint(fx, fy);
  juce::Point<float> worldMousePos(fx, fy);

  NodeBlock *targetBlock = nullptr;
  for (auto *block : nodeBlocks) {
    auto *node = block->getNode().get();
    if (node == droppedNode) {
      continue;
    }

    // Use world-space bounds from the node itself
    juce::Rectangle<float> worldBounds(node->nodeX, node->nodeY,
                                       (float)block->getWidth(),
                                       (float)block->getHeight());
    if (worldBounds.contains(worldMousePos)) {
      targetBlock = block;
      break;
    }
  }

  if (targetBlock == nullptr) {
    return;
  }

  GraphNode *targetNode = targetBlock->getNode().get();
  float targetWidth = (float)targetBlock->getWidth();
  float localX = worldMousePos.x - targetNode->nodeX;

  bool connected = false;
  if (localX < targetWidth * 0.25f) {
    // Left 1/4 of Target: Dropped Output(s) -> Target Input(s)
    int limit = std::min(droppedNode->getNumOutputPorts(),
                         targetNode->getNumInputPorts());
    for (int i = 0; i < limit; ++i) {
      if (!graphEngine.isInputPortOccupied(targetNode, i)) {
        if (graphEngine.addExplicitConnection(droppedNode, i, targetNode, i)) {
          connected = true;
        }
      }
    }
  } else if (localX > targetWidth * 0.75f) {
    // Right 1/4 of Target: Target Output(s) -> Dropped Input(s)
    int limit = std::min(targetNode->getNumOutputPorts(),
                         droppedNode->getNumInputPorts());
    for (int i = 0; i < limit; ++i) {
      if (!graphEngine.isInputPortOccupied(droppedNode, i)) {
        if (graphEngine.addExplicitConnection(targetNode, i, droppedNode, i)) {
          connected = true;
        }
      }
    }
  }

  if (connected) {
    if (performMutation) {
      // Actually, proximity connection happens after a drag-mutation is already
      // finished? No, it's called during mouseUp. We should probably wrap the
      // entire mouseUp sequence in a mutation if any change happens.
    }
    refreshCableCache();
    repaint();
    if (onGraphChanged) {
      onGraphChanged();
    }
  }
}

bool GraphCanvas::attemptSignalPathInsertion(GraphNode *newNode,
                                             juce::Point<int> mousePos) {
  if (newNode == nullptr) {
    return false;
  }

  // Transform screen to world
  float fx = (float)mousePos.x;
  float fy = (float)mousePos.y;
  getCameraTransform().inverted().transformPoint(fx, fy);
  juce::Point<float> worldMousePos(fx, fy);

  // High threshold for "near" a cable (15 world-space pixels)
  const float threshold = 15.0f;
  CachedCable *bestCable = nullptr;
  float bestDist = threshold;

  for (auto &cable : cachedCables) {
    juce::Point<float> nearest;
    cable.path.getNearestPoint(worldMousePos, nearest);
    float d = worldMousePos.getDistanceFrom(nearest);
    if (d < bestDist) {
      bestDist = d;
      bestCable = &cable;
    }
  }

  if (bestCable != nullptr) {
    GraphNode *sourceNode = bestCable->sourceNode;
    int sourcePort = bestCable->sourcePort;
    GraphNode *targetNode = bestCable->targetNode;
    int targetPort = bestCable->targetPort;

    // Disconnect A -> B. Then A -> New(0), New(0) -> B
    if (performMutation) {
      performMutation([this, sourceNode, sourcePort, targetNode, targetPort,
                       newNode]() {
        graphEngine.removeConnection(sourceNode, sourcePort, targetNode,
                                     targetPort);

        graphEngine.addExplicitConnection(sourceNode, sourcePort, newNode, 0);
        graphEngine.addExplicitConnection(newNode, 0, targetNode, targetPort);
      });
      refreshCableCache();
      repaint();
      if (onGraphChanged) {
        onGraphChanged();
      }
      return true;
    } else {
      graphEngine.removeConnection(sourceNode, sourcePort, targetNode,
                                   targetPort);

      bool conn1 =
          graphEngine.addExplicitConnection(sourceNode, sourcePort, newNode, 0);
      bool conn2 =
          graphEngine.addExplicitConnection(newNode, 0, targetNode, targetPort);

      if (conn1 || conn2) {
        refreshCableCache();
        repaint();
        if (onGraphChanged) {
          onGraphChanged();
        }
        return true;
      }
    }
  }

  return false;
}

// Find target port under the release point
void GraphCanvas::endCableDrag(juce::Point<int> canvasPos) {
  if (!isDraggingCable) {
    return;
  }

  isDraggingCable = false;
  repaint();

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
          if (!graphEngine.checkPortTypeCompatibility(
                  cableDragSourceBlock->getNode().get(), cableDragSourcePort,
                  block->getNode().get(), i)) {
            rejectFlashPos = canvasPos.toFloat();
            rejectFlashAlpha = 1.0f;
            startTimerHz(30);
          } else if (performMutation) {
            performMutation([this, block, i]() {
              graphEngine.addExplicitConnection(
                  cableDragSourceBlock->getNode().get(), cableDragSourcePort,
                  block->getNode().get(), i);
            });
          } else {
            graphEngine.addExplicitConnection(
                cableDragSourceBlock->getNode().get(), cableDragSourcePort,
                block->getNode().get(), i);
          }
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
          if (!graphEngine.checkPortTypeCompatibility(
                  block->getNode().get(), i,
                  cableDragSourceBlock->getNode().get(), cableDragSourcePort)) {
            rejectFlashPos = canvasPos.toFloat();
            rejectFlashAlpha = 1.0f;
            startTimerHz(30);
          } else if (performMutation) {
            performMutation([this, block, i]() {
              graphEngine.addExplicitConnection(
                  block->getNode().get(), i,
                  cableDragSourceBlock->getNode().get(), cableDragSourcePort);
            });
          } else {
            graphEngine.addExplicitConnection(
                block->getNode().get(), i,
                cableDragSourceBlock->getNode().get(), cableDragSourcePort);
          }
          break;
        }
      }
    }
  }

  cableDragSourceBlock = nullptr;
  cableDragSourcePort = -1;
  refreshCableCache();
  repaint();
}

NodeBlock *GraphCanvas::findBlockForNode(GraphNode *node) const {
  for (auto *block : nodeBlocks) {
    if (block->getNode().get() == node) {
      return block;
    }
  }
  return nullptr;
}

void GraphCanvas::resized() { updateScrollBars(); }

bool GraphCanvas::keyPressed(const juce::KeyPress &key,
                             juce::Component *originatingComponent) {
  juce::ignoreUnused(originatingComponent);

  if (key.isKeyCode(juce::KeyPress::deleteKey) ||
      key.isKeyCode(juce::KeyPress::backspaceKey)) {
    // If the focused component is a TextEditor, let it handle the key
    if (dynamic_cast<juce::TextEditor *>(originatingComponent) != nullptr) {
      return false;
    }

    if (!selectedNodes.empty()) {
      // Capture a copy so the lambda can use it safely
      std::vector<GraphNode *> toRemove(selectedNodes.begin(), selectedNodes.end());
      selectedNodes.clear();
      const juce::ScopedLock sl(graphLock);
      if (performMutation) {
        performMutation([this, toRemove]() {
          for (auto *n : toRemove) {
            graphEngine.removeNode(n);
          }
        });
      } else {
        for (auto *n : toRemove) {
          graphEngine.removeNode(n);
        }
      }
      rebuild();
      if (onGraphChanged) {
        onGraphChanged();
      }
      return true;
    }
  }

  if (key.isKeyCode(juce::KeyPress::escapeKey)) {
    // Cancel cable dragging
    if (isDraggingCable) {
      endCableDrag(cableDragEnd);  // This cleans up state
      return true;
    }

    // Cancel any node dragging
    for (auto *block : nodeBlocks) {
      block->cancelDrag();
    }
    return true;
  }

  // Ctrl+Z / Cmd+Z to undo
  if ((key.getKeyCode() == 'z' || key.getKeyCode() == 'Z') &&
      key.getModifiers().isCommandDown()) {
    if (key.getModifiers().isShiftDown()) {
      redo();
    } else {
      undo();
    }
    return true;
  }

  // Ctrl+D / Cmd+D to clone selected node(s)
  if ((key.getKeyCode() == 'd' || key.getKeyCode() == 'D') &&
      key.getModifiers().isCommandDown()) {
    if (!selectedNodes.empty()) {
      for (auto *node : selectedNodes) {
        auto nearest = graphEngine.findClosestFreeSpot(
            node->gridX + node->getGridWidth(), node->gridY,
            node->getGridWidth(), node->getGridHeight(), nullptr,
            getViewportGridCenter());
        requestNodeClone(node, nearest.x, nearest.y);
      }
      return true;
    }
  }

  // Space to toggle transport
  if (key.getKeyCode() == juce::KeyPress::spaceKey ||
      key.getTextCharacter() == ' ') {
    if (onTransportToggle) {
      onTransportToggle();
      return true;
    }
  }

  return false;
}

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
  if (getWidth() <= 0 || getHeight() <= 0) {
    return;
  }

  float minX = 0.0f;
  float minY = 0.0f;
  float maxX = 0.0f;
  float maxY = 0.0f;
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
  if (nodeBlocks.isEmpty()) {
    return;
  }

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

  if (worldW <= 0.0f || worldH <= 0.0f) {
    return;
  }

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

  for (const auto &cable : cachedCables) {
    juce::Point<float> nearest;
    cable.path.getNearestPoint(localPosWorld, nearest);
    if (nearest.getDistanceFrom(localPosWorld) < 12.0f) {
      cableTooltipText = "Steps: " + juce::String(cable.stepCount) +
                         " | Active: " + juce::String(cable.activeStepCount);
      if (cable.isLarge) {
        cableTooltipText += juce::String::fromUTF8(" \xe2\x9a\xa0");
      }
      // position tooltip at actual cursor screen coordinate
      cableTooltipPos = e.getPosition();
      showCableTooltip = true;
      found = true;
      break;
    }
  }

  if (!found && showCableTooltip) {
    showCableTooltip = false;
  }

  repaint();
}

void GraphCanvas::mouseExit(const juce::MouseEvent & /*event*/) {
  if (showCableTooltip) {
    showCableTooltip = false;
    repaint();
  }
}

void GraphCanvas::checkForLargeSequences() {
  bool foundLarge = false;
  const juce::ScopedLock sl(graphLock);
  const auto &nodes = graphEngine.getNodes();

  for (const auto &node : nodes) {
    for (const auto &[outPort, connVec] : node->getConnections()) {
      const auto &outSeq = node->getOutputSequence(outPort);
      if (outSeq.size() > 10000) {
        foundLarge = true;
        break;
      }
    }
    if (foundLarge) {
      break;
    }
  }

  if (foundLarge != hasLargeSequenceWarning) {
    hasLargeSequenceWarning = foundLarge;
    repaint();
  }
}

void GraphCanvas::addNodeAtPosition(const std::shared_ptr<GraphNode> &node,
                                    juce::Point<int> screenPos) {
  juce::ScopedLock l(graphLock);

  // Convert screen drop pixel to internal world position considering pan &
  // zoom
  float worldX = (float)screenPos.x;
  float worldY = (float)screenPos.y;
  getCameraTransform().inverted().transformPoint(worldX, worldY);

  int dropGridX;
  int dropGridY;

  if (showGhostTarget && ghostIsValid) {
    dropGridX = ghostTargetX;
    dropGridY = ghostTargetY;
  } else {
    int ix = (int)std::round(worldX / Layout::GridPitchFloat);
    int iy = (int)std::round(worldY / Layout::GridPitchFloat);
    auto nearest = graphEngine.findClosestFreeSpot(
        ix, iy, node->getGridWidth(), node->getGridHeight(), nullptr,
        getViewportGridCenter());
    dropGridX = nearest.x;
    dropGridY = nearest.y;
  }

  node->gridX = dropGridX;
  node->gridY = dropGridY;

  node->nodeX =
      (float)(node->gridX * Layout::GridPitch) + Layout::TramlineOffset;
  node->nodeY =
      (float)(node->gridY * Layout::GridPitch) + Layout::TramlineOffset;

  graphEngine.addNode(node);

  // Rebuild the NodeBlock wrappers
  rebuild();

  // Recalculate and apply scaled block bounds cleanly derived from
  // nodeX/nodeY
  updateTransforms();

  if (onGraphChanged) {
    onGraphChanged();
  }
}

void GraphCanvas::updateProximityHighlight(juce::Point<int> mousePos,
                                           GraphNode *draggingNode) {
  GraphNode *newTarget = nullptr;
  ProximityZone newZone = ProximityZone::None;
  CableID bestCableID;

  float fx = (float)mousePos.x;
  float fy = (float)mousePos.y;
  getCameraTransform().inverted().transformPoint(fx, fy);
  juce::Point<float> worldMousePos(fx, fy);

  // 1. Check Cable Insertion first (precedence)
  const float cableThreshold = 15.0f;
  float bestDist = cableThreshold;
  for (auto &cable : cachedCables) {
    juce::Point<float> nearest;
    cable.path.getNearestPoint(worldMousePos, nearest);
    float d = worldMousePos.getDistanceFrom(nearest);
    if (d < bestDist) {
      bestDist = d;
      bestCableID = {cable.sourceNode, cable.sourcePort, cable.targetNode,
                     cable.targetPort};
    }
  }

  // 2. If no cable, check Node Proximity
  if (!bestCableID.isValid()) {
    for (auto *block : nodeBlocks) {
      auto *node = block->getNode().get();
      if (node == draggingNode) {
        continue;
      }

      // Use world-space bounds from the node itself
      juce::Rectangle<float> worldBounds(node->nodeX, node->nodeY,
                                         (float)block->getWidth(),
                                         (float)block->getHeight());
      if (worldBounds.contains(worldMousePos)) {
        newTarget = node;
        float targetWidth = (float)block->getWidth();
        float localX = worldMousePos.x - node->nodeX;

        if (localX < targetWidth * 0.25f) {
          newZone = ProximityZone::Left;
        } else if (localX > targetWidth * 0.75f) {
          newZone = ProximityZone::Right;
        }
        break;
      }
    }
  }

  if (newTarget != proximityTargetNode || newZone != proximityZone ||
      bestCableID.sourceNode != proximityCableID.sourceNode ||
      bestCableID.sourcePort != proximityCableID.sourcePort ||
      bestCableID.targetNode != proximityCableID.targetNode ||
      bestCableID.targetPort != proximityCableID.targetPort) {
    proximityTargetNode = newTarget;
    proximityZone = newZone;
    proximityCableID = bestCableID;
    repaint();
  }
}

void GraphCanvas::clearProximityHighlight() {
  if (proximityTargetNode != nullptr || proximityZone != ProximityZone::None ||
      proximityCableID.isValid()) {
    proximityTargetNode = nullptr;
    proximityZone = ProximityZone::None;
    proximityCableID.clear();
    repaint();
  }
}

bool GraphCanvas::isInterestedInDragSource(const SourceDetails &details) {
  return details.description.toString().isNotEmpty();
}

void GraphCanvas::itemDragMove(const SourceDetails &details) {
  juce::String nodeType = details.description.toString();
  auto meta = NodeFactory::getPreviewMetadata(nodeType.toStdString());

  // Calculate grid position for ghost
  float fx = (float)details.localPosition.x;
  float fy = (float)details.localPosition.y;
  getCameraTransform().inverted().transformPoint(fx, fy);

  // Calculate top-left based on centered drag image
  float halfW = (meta.gridW * Layout::GridPitchFloat) / 2.0f;
  float halfH = (meta.gridH * Layout::GridPitchFloat) / 2.0f;

  int gx = juce::roundToInt((fx - halfW - Layout::TramlineOffset) /
                            Layout::GridPitchFloat);
  int gy = juce::roundToInt((fy - halfH - Layout::TramlineOffset) /
                            Layout::GridPitchFloat);

  setGhostTarget(gx, gy, meta.gridW, meta.gridH, nullptr);

  updateProximityHighlight(details.localPosition, nullptr);
}

void GraphCanvas::itemDragExit(const SourceDetails &details) {
  juce::ignoreUnused(details);
  clearGhostTarget();
  clearProximityHighlight();
}

void GraphCanvas::itemDropped(const SourceDetails &details) {
  if (onNodeDropped) {
    onNodeDropped(details.description.toString(), details.localPosition);
  }
  clearGhostTarget();
  clearProximityHighlight();
}

void GraphCanvas::selectNode(GraphNode *node) {
  bool changed = !(selectedNodes.size() == 1 && selectedNodes.count(node) > 0);
  selectedNodes.clear();
  if (node != nullptr) {
    selectedNodes.insert(node);
  }
  grabKeyboardFocus();
  if (changed) {
    refreshCableCache();
    repaint();
  }
}

void GraphCanvas::addToSelection(GraphNode *node) {
  if (selectedNodes.count(node) > 0) {
    selectedNodes.erase(node);
  } else {
    selectedNodes.insert(node);
  }
  refreshCableCache();
  repaint();
}

void GraphCanvas::clearSelection() {
  if (!selectedNodes.empty()) {
    selectedNodes.clear();
    refreshCableCache();
    repaint();
  }
}

// ---------------------------------------------------------------------------
// Group drag
// ---------------------------------------------------------------------------

void GraphCanvas::setGroupGhostTarget(int bboxGridX, int bboxGridY) {
  bool posChanged = (bboxGridX != ghostTargetX || bboxGridY != ghostTargetY);

  showGhostTarget = true;
  ghostTargetX = bboxGridX;
  ghostTargetY = bboxGridY;
  ghostTargetW = groupDragBBoxW;
  ghostTargetH = groupDragBBoxH;

  const juce::ScopedLock sl(graphLock);
  ghostIsValid = !graphEngine.isAreaOccupied(bboxGridX, bboxGridY,
                                              groupDragBBoxW, groupDragBBoxH,
                                              selectedNodes);
  if (ghostIsValid) {
    ghostResolvedX = bboxGridX;
    ghostResolvedY = bboxGridY;
    hasGhostResolved = true;
  } else if (posChanged || !hasGhostResolved) {
    // Find nearest free spot for the bounding box, ignoring all selected nodes
    GraphNode *firstSelected = selectedNodes.empty() ? nullptr : *selectedNodes.begin();
    auto resolved = graphEngine.findClosestFreeSpot(
        bboxGridX, bboxGridY, groupDragBBoxW, groupDragBBoxH,
        firstSelected, getViewportGridCenter());
    ghostResolvedX = resolved.x;
    ghostResolvedY = resolved.y;
    hasGhostResolved = true;
  }
  repaint();
}

void GraphCanvas::beginGroupDrag(GraphNode *anchor, const juce::MouseEvent &e) {
  juce::ignoreUnused(e);
  isGroupDragging_ = true;
  groupDragAnchor = anchor;
  groupDragNodes.clear();
  groupDragCurrentDeltaX = 0;
  groupDragCurrentDeltaY = 0;

  int minX = std::numeric_limits<int>::max();
  int minY = std::numeric_limits<int>::max();
  int maxX = std::numeric_limits<int>::min();
  int maxY = std::numeric_limits<int>::min();

  for (auto *node : selectedNodes) {
    GroupDragNodeData d;
    d.node = node;
    d.startGridX = node->gridX;
    d.startGridY = node->gridY;
    d.startWorldX = node->nodeX;
    d.startWorldY = node->nodeY;
    groupDragNodes.push_back(d);

    minX = std::min(minX, node->gridX);
    minY = std::min(minY, node->gridY);
    maxX = std::max(maxX, node->gridX + node->getGridWidth());
    maxY = std::max(maxY, node->gridY + node->getGridHeight());
  }

  groupDragBBoxMinX = minX;
  groupDragBBoxMinY = minY;
  groupDragBBoxW = maxX - minX;
  groupDragBBoxH = maxY - minY;

  setGroupGhostTarget(minX, minY);
}

void GraphCanvas::updateGroupDrag(const juce::MouseEvent &e) {
  if (!isGroupDragging_) {
    return;
  }

  int deltaX = e.getDistanceFromDragStartX();
  int deltaY = e.getDistanceFromDragStartY();
  groupDragCurrentDeltaX =
      (int)std::round((float)deltaX / Layout::GridPitchFloat);
  groupDragCurrentDeltaY =
      (int)std::round((float)deltaY / Layout::GridPitchFloat);

  const bool isClone = e.mods.isCtrlDown();
  for (auto &d : groupDragNodes) {
    if (isClone) {
      // Original stays in place
      d.node->nodeX = d.startWorldX;
      d.node->nodeY = d.startWorldY;
    } else {
      d.node->nodeX = (static_cast<float>(d.startGridX) * Layout::GridPitchFloat) +
                      Layout::TramlineOffset + static_cast<float>(deltaX);
      d.node->nodeY = (static_cast<float>(d.startGridY) * Layout::GridPitchFloat) +
                      Layout::TramlineOffset + static_cast<float>(deltaY);
    }
  }

  setGroupGhostTarget(groupDragBBoxMinX + groupDragCurrentDeltaX,
                      groupDragBBoxMinY + groupDragCurrentDeltaY);
  updateTransforms();
  refreshCableCache();
  repaint();
}

void GraphCanvas::commitGroupDrag(bool isClone) {
  if (!isGroupDragging_) {
    return;
  }
  isGroupDragging_ = false;

  // Compute the resolve offset (if bounding box had a collision)
  int resolveOffsetX = 0;
  int resolveOffsetY = 0;
  if (!ghostIsValid && hasGhostResolved) {
    int requestedBBoxX = groupDragBBoxMinX + groupDragCurrentDeltaX;
    int requestedBBoxY = groupDragBBoxMinY + groupDragCurrentDeltaY;
    resolveOffsetX = ghostResolvedX - requestedBBoxX;
    resolveOffsetY = ghostResolvedY - requestedBBoxY;
  }

  int finalDeltaX = groupDragCurrentDeltaX + resolveOffsetX;
  int finalDeltaY = groupDragCurrentDeltaY + resolveOffsetY;

  clearGhostTarget();

  if (isClone) {
    // Revert all nodes to their start positions, then clone each to the
    // computed target position.
    for (auto &d : groupDragNodes) {
      d.node->nodeX = d.startWorldX;
      d.node->nodeY = d.startWorldY;
    }
    updateTransforms();

    for (auto &d : groupDragNodes) {
      int targetX = d.startGridX + finalDeltaX;
      int targetY = d.startGridY + finalDeltaY;
      requestNodeClone(d.node, targetX, targetY);
    }
  } else {
    if (performMutation) {
      performMutation([this, finalDeltaX, finalDeltaY]() {
        for (auto &d : groupDragNodes) {
          d.node->gridX = d.startGridX + finalDeltaX;
          d.node->gridY = d.startGridY + finalDeltaY;
          d.node->nodeX = (float)(d.node->gridX * Layout::GridPitch) +
                          Layout::TramlineOffset;
          d.node->nodeY = (float)(d.node->gridY * Layout::GridPitch) +
                          Layout::TramlineOffset;
        }
      });
    } else {
      for (auto &d : groupDragNodes) {
        d.node->gridX = d.startGridX + finalDeltaX;
        d.node->gridY = d.startGridY + finalDeltaY;
        d.node->nodeX = (float)(d.node->gridX * Layout::GridPitch) +
                        Layout::TramlineOffset;
        d.node->nodeY = (float)(d.node->gridY * Layout::GridPitch) +
                        Layout::TramlineOffset;
      }
    }
    updateTransforms();
    refreshCableCache();
    repaint();
    if (onGraphChanged) {
      onGraphChanged();
    }
  }

  groupDragNodes.clear();
  groupDragAnchor = nullptr;
}

void GraphCanvas::cancelGroupDrag() {
  if (!isGroupDragging_) {
    return;
  }
  isGroupDragging_ = false;
  for (auto &d : groupDragNodes) {
    d.node->nodeX = d.startWorldX;
    d.node->nodeY = d.startWorldY;
  }
  clearGhostTarget();
  updateTransforms();
  refreshCableCache();
  repaint();
  groupDragNodes.clear();
  groupDragAnchor = nullptr;
}

void GraphCanvas::setGhostTarget(int gridX, int gridY, int gridW, int gridH,
                                 GraphNode *ignoreNode) {
  bool posChanged = (gridX != ghostTargetX || gridY != ghostTargetY);

  showGhostTarget = true;
  ghostTargetX = gridX;
  ghostTargetY = gridY;
  ghostTargetW = gridW;
  ghostTargetH = gridH;

  const juce::ScopedLock sl(graphLock);
  ghostIsValid =
      !graphEngine.isAreaOccupied(gridX, gridY, gridW, gridH, ignoreNode);

  if (ghostIsValid) {
    ghostResolvedX = gridX;
    ghostResolvedY = gridY;
    hasGhostResolved = true;
  } else if (posChanged || !hasGhostResolved) {
    auto resolved = graphEngine.findClosestFreeSpot(
        gridX, gridY, gridW, gridH, ignoreNode, getViewportGridCenter());
    ghostResolvedX = resolved.x;
    ghostResolvedY = resolved.y;
    hasGhostResolved = true;
  }

  repaint();
}

void GraphCanvas::clearGhostTarget() {
  showGhostTarget = false;
  repaint();
}

void GraphCanvas::timerCallback() {
  rejectFlashAlpha *= 0.80f;
  if (rejectFlashAlpha < 0.01f) {
    rejectFlashAlpha = 0.0f;
    stopTimer();
  }
  repaint();
}
