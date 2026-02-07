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
        drawCable(g, start, end);
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
}

void GraphCanvas::drawCable(juce::Graphics &g, juce::Point<int> start,
                            juce::Point<int> end, bool highlighted) {
  juce::Path path;
  path.startNewSubPath(start.toFloat());

  float dx = std::max(std::abs((float)(end.x - start.x)) * 0.5f, 40.0f);
  path.cubicTo(start.x + dx, (float)start.y, end.x - dx, (float)end.y,
               (float)end.x, (float)end.y);

  if (highlighted) {
    // Bright yellow for in-progress drag
    g.setColour(juce::Colour(0xffeeee44));
    g.strokePath(path, juce::PathStrokeType(3.5f));
  } else {
    // Bright white-ish cable
    g.setColour(juce::Colour(0xffdddddd));
    g.strokePath(path, juce::PathStrokeType(3.0f));
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
