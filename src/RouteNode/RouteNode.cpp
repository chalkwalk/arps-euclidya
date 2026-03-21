#include "RouteNode.h"
#include <juce_gui_basics/juce_gui_basics.h>

// --- RouteNode Impl

RouteNode::RouteNode(std::array<std::atomic<float> *, 32> &inMacros)
    : macros(inMacros) {}

NodeLayout RouteNode::getLayout() const {
  NodeLayout layout;
  layout.gridWidth = 1;
  layout.gridHeight = 1;

  UIElement button;
  button.type = UIElementType::PushButton;
  button.label = (routeDest == 0) ? "OUT 0" : "OUT 1";
  button.valueRef = const_cast<int *>(&routeDest);
  button.macroIndexRef = const_cast<int *>(&macroRouteDest);
  button.gridBounds = {0, 0, 3, 1};
  layout.elements.push_back(button);

  return layout;
}

void RouteNode::saveNodeState(juce::XmlElement *xml) {
  if (xml) {
    xml->setAttribute("routeDest", routeDest);
    xml->setAttribute("macroRouteDest", macroRouteDest);
  }
}

void RouteNode::loadNodeState(juce::XmlElement *xml) {
  if (xml) {
    routeDest = xml->getIntAttribute("routeDest", 0);
    macroRouteDest = xml->getIntAttribute("macroRouteDest", -1);
  }
}

void RouteNode::process() {
  int actualDest =
      macroRouteDest != -1 && macros[(size_t)macroRouteDest] != nullptr
          ? (macros[(size_t)macroRouteDest]->load() >= 0.5f ? 1 : 0)
          : routeDest;

  auto it = inputSequences.find(0);
  NoteSequence emptySeq;
  NoteSequence inSeq = (it != inputSequences.end()) ? it->second : emptySeq;

  if (actualDest == 0) {
    outputSequences[0] = inSeq;
    outputSequences[1] = emptySeq;
  } else {
    outputSequences[0] = emptySeq;
    outputSequences[1] = inSeq;
  }

  auto conn0 = connections.find(0);
  if (conn0 != connections.end()) {
    for (const auto &c : conn0->second) {
      c.targetNode->setInputSequence(c.targetInputPort, outputSequences[0]);
    }
  }

  auto conn1 = connections.find(1);
  if (conn1 != connections.end()) {
    for (const auto &c : conn1->second) {
      c.targetNode->setInputSequence(c.targetInputPort, outputSequences[1]);
    }
  }
}
