#include "SelectNode.h"
#include <juce_gui_basics/juce_gui_basics.h>

// --- SelectNode Impl

SelectNode::SelectNode(std::array<std::atomic<float> *, 32> &inMacros)
    : macros(inMacros) {}

NodeLayout SelectNode::getLayout() const {
  NodeLayout layout;
  layout.gridWidth = 1;
  layout.gridHeight = 1;

  UIElement button;
  button.type = UIElementType::PushButton;
  button.label = (selectSource == 0) ? "IN 0" : "IN 1";
  button.valueRef = const_cast<int *>(&selectSource);
  button.macroIndexRef = const_cast<int *>(&macroSelectSource);
  button.gridBounds = {0, 0, 3, 1};
  layout.elements.push_back(button);

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
