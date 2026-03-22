#include "SelectNode.h"
#include "../LayoutParser.h"
#include "BinaryData.h"
#include <juce_gui_basics/juce_gui_basics.h>

// --- SelectNode Impl

SelectNode::SelectNode(std::array<std::atomic<float> *, 32> &inMacros)
    : macros(inMacros) {}

NodeLayout SelectNode::getLayout() const {
  auto layout = LayoutParser::parseFromJSON(BinaryData::SelectNode_json,
                                            BinaryData::SelectNode_jsonSize);

  // Bind runtime pointers by matching element labels
  for (auto &el : layout.elements) {
   if (el.label == "selectSource") {
      el.valueRef = const_cast<int *>(&selectSource);
      el.macroIndexRef = const_cast<int *>(&macroSelectSource);
    }
  }

  return layout;
}

void SelectNode::saveNodeState(juce::XmlElement *xml) {
  if (xml) {
    xml->setAttribute("selectSource", selectSource);
    xml->setAttribute("macroSelectSource", macroSelectSource);
  }
}

void SelectNode::loadNodeState(juce::XmlElement *xml) {
  if (xml) {
    selectSource = xml->getIntAttribute("selectSource", 0);
    macroSelectSource = xml->getIntAttribute("macroSelectSource", -1);
  }
}

void SelectNode::process() {
  int actualSource =
      macroSelectSource != -1 && macros[(size_t)macroSelectSource] != nullptr
          ? (macros[(size_t)macroSelectSource]->load() >= 0.5f ? 1 : 0)
          : selectSource;

  auto it0 = inputSequences.find(0);
  auto it1 = inputSequences.find(1);
  NoteSequence emptySeq;
  NoteSequence in0 = (it0 != inputSequences.end()) ? it0->second : emptySeq;
  NoteSequence in1 = (it1 != inputSequences.end()) ? it1->second : emptySeq;

  outputSequences[0] = (actualSource == 0) ? in0 : in1;

  auto conn = connections.find(0);
  if (conn != connections.end()) {
    for (const auto &c : conn->second) {
      c.targetNode->setInputSequence(c.targetInputPort, outputSequences[0]);
    }
  }
}
