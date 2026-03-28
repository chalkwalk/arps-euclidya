#include "MultiplyNode.h"

#include <algorithm>
#include <cmath>

#include "../LayoutParser.h"
#include "../MacroMappingMenu.h"
#include "../SharedMacroUI.h"
#include "BinaryData.h"

// --- MultiplyNode Impl

void MultiplyNode::process() {
  int actualRepeat =
      macroRepeatCount != -1 && macros[(size_t)macroRepeatCount] != nullptr
          ? 1 + (int)std::round(macros[(size_t)macroRepeatCount]->load() *
                                15.0f)
          : repeatCount;

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
    xml->setAttribute("macroRepeatCount", macroRepeatCount);
  }
}

void MultiplyNode::loadNodeState(juce::XmlElement *xml) {
  if (xml) {
    repeatCount = xml->getIntAttribute("repeatCount", 2);
    macroRepeatCount = xml->getIntAttribute("macroRepeatCount", -1);
  }
}

NodeLayout MultiplyNode::getLayout() const {
  auto layout = LayoutParser::parseFromJSON(BinaryData::MultiplyNode_json,
                                            BinaryData::MultiplyNode_jsonSize);

  // Bind runtime pointers by matching element labels
  for (auto &el : layout.elements) {
    if (el.label == "repeatCount") {
      el.valueRef = const_cast<int *>(&repeatCount);
      el.macroIndexRef = const_cast<int *>(&macroRepeatCount);
    }
  }

  return layout;
}
