#include "MultiplyNode.h"

#include <algorithm>
#include <cmath>

#include "../LayoutParser.h"
#include "../SharedMacroUI.h"
#include "BinaryData.h"

// --- MultiplyNode Impl

void MultiplyNode::process() {
  int actualRepeat = resolveMacroInt(macroRepeatCount, repeatCount, 0, 16);
  actualRepeat = std::max(1, actualRepeat);

  auto it = inputSequences.find(0);
  if (it == inputSequences.end() || it->second.empty()) {
    outputSequences[0] = {};
  } else {
    NoteSequence out;
    for (const auto &step : it->second) {
      for (int r = 0; r < actualRepeat; ++r) {
        out.push_back(step);
      }
    }
    outputSequences[0] = out;
  }

  auto conn = connections.find(0);
  if (conn != connections.end()) {
    for (const auto &c : conn->second) {
      c.targetNode->setInputSequence(c.targetInputPort, outputSequences[0]);
    }
  }
}

void MultiplyNode::saveNodeState(juce::XmlElement *xml) {
  if (xml) {
    xml->setAttribute("repeatCount", repeatCount);
    saveMacroBindings(xml);
  }
}

void MultiplyNode::loadNodeState(juce::XmlElement *xml) {
  if (xml) {
    repeatCount = xml->getIntAttribute("repeatCount", 2);
    if (xml->hasAttribute("macroRepeatCount")) {
      int m = xml->getIntAttribute("macroRepeatCount", -1);
      if (m != -1)
        macroRepeatCount.bindings.push_back({m, 1.0f});
    }
    loadMacroBindings(xml);
  }
}

NodeLayout MultiplyNode::getLayout() const {
  auto layout = LayoutParser::parseFromJSON(BinaryData::MultiplyNode_json,
                                            BinaryData::MultiplyNode_jsonSize);

  // Bind runtime pointers by matching element labels
  for (auto &el : layout.elements) {
    if (el.label == "repeatCount") {
      el.valueRef = const_cast<int *>(&repeatCount);
      el.macroParamRef = const_cast<MacroParam *>(&macroRepeatCount);
    }
  }

  return layout;
}
