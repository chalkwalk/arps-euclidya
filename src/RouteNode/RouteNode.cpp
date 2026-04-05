#include "RouteNode.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include "../LayoutParser.h"
#include "BinaryData.h"

// --- RouteNode Impl

NodeLayout RouteNode::getLayout() const {
  auto layout = LayoutParser::parseFromJSON(BinaryData::RouteNode_json,
                                            BinaryData::RouteNode_jsonSize);

  // Bind runtime pointers by matching element labels
  for (auto &el : layout.elements) {
    if (el.label == "routeDest") {
      el.valueRef = const_cast<int *>(&routeDest);
      el.macroParamRef = const_cast<MacroParam *>(&macroRouteDest);
    }
  }

  return layout;
}

void RouteNode::saveNodeState(juce::XmlElement *xml) {
  if (xml) {
    xml->setAttribute("routeDest", routeDest);
    saveMacroBindings(xml);
  }
}

void RouteNode::loadNodeState(juce::XmlElement *xml) {
  if (xml) {
    routeDest = xml->getIntAttribute("routeDest", 0);
    if (xml->hasAttribute("macroRouteDest")) {
      int m = xml->getIntAttribute("macroRouteDest", -1);
      if (m != -1)
        macroRouteDest.bindings.push_back({m, 1.0f});
    }
    loadMacroBindings(xml);
  }
}

void RouteNode::process() {
  int actualDest = resolveMacroInt(macroRouteDest, routeDest, 0, 1);

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
