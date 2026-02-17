#include "GraphCanvas.h"

GraphCanvas::GraphCanvas(GraphEngine &engine,
                         juce::AudioProcessorValueTreeState &apvtsRef,
                         juce::CriticalSection &lock)
    : graphEngine(engine), apvts(apvtsRef), graphLock(lock) {}

void GraphCanvas::rebuild() {
  nodeBlocks.clear();

  const juce::ScopedLock sl(graphLock);
  auto &nodes = graphEngine.getNodes();

  for (auto &node : nodes) {
    auto *block = new NodeBlock(node, apvts, *this);

    block->setTopLeftPosition((int)node->nodeX, (int)node->nodeY);

    block->onDelete = [this, nodePtr = node.get()]() {
      const juce::ScopedLock sl2(graphLock);
      graphEngine.removeNode(nodePtr);
      rebuild();
      if (onGraphChanged)
        onGraphChanged();
    };

    block->onPositionChanged = [this]() {
      updateCanvasSize();
      repaint();
    };

    // Selection: bring to front when clicked anywhere on the block
    block->onSelected = [this, b = block]() {
      selectNode(b->getNode().get());
      b->toFront(true);
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

  updateCanvasSize();
  repaint();
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

  // Draw grid dots
  g.setColour(juce::Colour(0xff2a2a2a));
  for (int x = 0; x < getWidth(); x += 20) {
    for (int y = 0; y < getHeight(); y += 20) {
      g.fillRect(x, y, 1, 1);
    }
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
    drawCable(g, start, cableDragEnd, true);
  }

  // Draw cable hover tooltip
  if (showCableTooltip) {
    auto font = juce::Font(12.0f);
    int textW = (int)font.getStringWidthFloat(cableTooltipText) + 12;
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
  // Right-click on canvas background to delete a cable near the click
  if (e.mods.isRightButtonDown()) {
    auto localPos = e.getPosition();
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

          // Use path proximity
          juce::Point<float> nearest;
          path.getNearestPoint(localPos.toFloat(), nearest);
          if (nearest.getDistanceFrom(localPos.toFloat()) < 12.0f) {
            graphEngine.removeConnection(node.get(), outPort, conn.targetNode,
                                         conn.targetInputPort);
            repaint();
            return;
          }
        }
      }
    }
  }
}

void GraphCanvas::startCableDrag(NodeBlock *block, int portIndex, bool isOutput,
                                 juce::Point<int> canvasPos) {
  isDraggingCable = true;
  cableDragSourceBlock = block;
  cableDragSourcePort = portIndex;
  cableDragFromOutput = isOutput;
  cableDragEnd = canvasPos;
  repaint();
}

void GraphCanvas::updateCableDrag(juce::Point<int> canvasPos) {
  if (isDraggingCable) {
    cableDragEnd = canvasPos;
    repaint();
  }
}

void GraphCanvas::endCableDrag(juce::Point<int> canvasPos) {
  if (!isDraggingCable)
    return;

  isDraggingCable = false;

  // Find target port under the release point
  for (auto *block : nodeBlocks) {
    auto blockLocal = canvasPos - block->getPosition();

    if (cableDragFromOutput) {
      // Dragging from output → looking for an input port
      int numIn = block->getNode()->getNumInputPorts();
      for (int i = 0; i < numIn; ++i) {
        auto portCentre = block->getInputPortCentre(i) - block->getPosition();
        if (blockLocal.getDistanceFrom(portCentre) <=
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
        auto portCentre = block->getOutputPortCentre(i) - block->getPosition();
        if (blockLocal.getDistanceFrom(portCentre) <=
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

void GraphCanvas::updateCanvasSize() {
  int maxX = 800;
  int maxY = 600;

  for (auto *block : nodeBlocks) {
    maxX = std::max(maxX, block->getRight() + 50);
    maxY = std::max(maxY, block->getBottom() + 50);
  }

  setSize(maxX, maxY);
}

void GraphCanvas::mouseMove(const juce::MouseEvent &e) {
  auto localPos = e.getPosition();
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
        path.getNearestPoint(localPos.toFloat(), nearest);
        if (nearest.getDistanceFrom(localPos.toFloat()) < 12.0f) {
          auto &outSeq = node->getOutputSequence(outPort);
          int stepCount = (int)outSeq.size();
          cableTooltipText = juce::String(stepCount) + " steps";
          if (stepCount > 10000)
            cableTooltipText += juce::String::fromUTF8(" \xe2\x9a\xa0");
          cableTooltipPos = localPos;
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

void GraphCanvas::selectNode(GraphNode *node) {
  if (selectedNode != node) {
    selectedNode = node;
    repaint();
  }
}
