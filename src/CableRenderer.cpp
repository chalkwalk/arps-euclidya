#include "CableRenderer.h"

#include "ArpsLookAndFeel.h"

CableRenderer::CableRenderer(GraphEngine &engine, juce::CriticalSection &lock,
                             PortPosFn portPosFn)
    : graphEngine(engine), graphLock(lock), getPortPos(std::move(portPosFn)) {}

void CableRenderer::refresh(
    const std::unordered_set<GraphNode *> &selectedNodes) {
  cachedCables.clear();
  const juce::ScopedLock sl(graphLock);
  const auto &nodes = graphEngine.getNodes();

  for (const auto &node : nodes) {
    for (const auto &[outPort, connVec] : node->getConnections()) {
      for (const auto &conn : connVec) {
        auto start = getPortPos(node.get(), outPort, true);
        auto end = getPortPos(conn.targetNode, conn.targetInputPort, false);

        // Port not found (NodeBlock not yet created) — skip
        if (!start || !end)
          continue;

        CachedCable cable;
        cable.sourceNode = node.get();
        cable.sourcePort = outPort;
        cable.targetNode = conn.targetNode;
        cable.targetPort = conn.targetInputPort;

        auto startF = start->toFloat();
        auto endF = end->toFloat();

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

        cable.portType =
            graphEngine.getEffectiveOutputPortType(node.get(), outPort);
        const auto &outSeq = node->getOutputSequence(outPort);
        cable.stepCount = (int)outSeq.size();
        cable.activeStepCount = 0;
        for (const auto &step : outSeq) {
          if (!step.empty())
            cable.activeStepCount++;
        }
        cable.isLarge = (cable.stepCount > 10000);
        cable.isSelected = !selectedNodes.empty() &&
                           (selectedNodes.count(node.get()) > 0 ||
                            selectedNodes.count(conn.targetNode) > 0);

        cachedCables.push_back(std::move(cable));
      }
    }
  }
}

bool CableRenderer::hasLargeSequence() const {
  for (const auto &c : cachedCables) {
    if (c.isLarge)
      return true;
  }
  return false;
}

void CableRenderer::drawCable(juce::Graphics &g, const juce::Path &path,
                              bool highlighted, bool warning, bool isForeground,
                              GraphNode::PortType portType,
                              bool hasSelection) const {
  auto shadowPath = path;
  shadowPath.applyTransform(juce::AffineTransform::translation(1.0f, 1.5f));
  g.setColour(juce::Colours::black.withAlpha(0.4f));
  g.strokePath(shadowPath, juce::PathStrokeType(2.5f));

  juce::Colour baseColor;
  if (highlighted) {
    baseColor = juce::Colour(0xffeeee44);
  } else if (warning) {
    baseColor = juce::Colour(0xffff6633);
  } else if (portType == GraphNode::PortType::CC) {
    baseColor = juce::Colour(0xffaa44ff);
  } else if (portType == GraphNode::PortType::Notes) {
    baseColor = juce::Colour(0xffd4a017);
  } else {
    baseColor = juce::Colour(0xffaaaaaa);
  }

  if (!isForeground && !highlighted && hasSelection) {
    baseColor = baseColor.withMultipliedAlpha(0.4f);
  }

  if (isForeground || highlighted || (!hasSelection && !warning)) {
    g.setColour(baseColor.withAlpha(0.15f));
    g.strokePath(path, juce::PathStrokeType(8.0f));
    g.setColour(ArpsLookAndFeel::getNeonColor().withAlpha(0.2f));
    g.strokePath(path, juce::PathStrokeType(5.0f));
  }

  g.setColour(baseColor);
  float strokeThickness = (isForeground || highlighted) ? 3.0f : 2.0f;
  if (highlighted)
    strokeThickness = 3.5f;
  g.strokePath(path, juce::PathStrokeType(strokeThickness));

  if (isForeground || highlighted) {
    g.setColour(juce::Colours::white.withAlpha(0.5f));
    g.strokePath(path, juce::PathStrokeType(1.0f));
  }
}

bool CableRenderer::checkForLargeSequences() {
  const juce::ScopedLock sl(graphLock);
  const auto &nodes = graphEngine.getNodes();
  for (const auto &node : nodes) {
    for (const auto &[outPort, connVec] : node->getConnections()) {
      if (node->getOutputSequence(outPort).size() > 10000)
        return true;
    }
  }
  return false;
}
