#include "SwitchNode.h"
#include <juce_gui_basics/juce_gui_basics.h>

// --- SwitchNode Impl

SwitchNode::SwitchNode(std::array<std::atomic<float> *, 32> &inMacros)
    : macros(inMacros) {}

NodeLayout SwitchNode::getLayout() const {
  NodeLayout layout;
  layout.gridWidth = 1;
  layout.gridHeight = 1;

  UIElement button;
  button.type = UIElementType::Toggle;
  button.label = "ENABLE";
  button.valueRef = const_cast<int *>(&switchOn);
  button.macroIndexRef = const_cast<int *>(&macroSwitch);
  button.gridBounds = {0, 0, 3, 1};
  layout.elements.push_back(button);

  return layout;
}

void SwitchNode::saveNodeState(juce::XmlElement *xml) {
  if (xml) {
    xml->setAttribute("switchOn", (bool)(switchOn != 0));
    xml->setAttribute("macroSwitch", macroSwitch);
  }
}

void SwitchNode::loadNodeState(juce::XmlElement *xml) {
  if (xml) {
    switchOn = xml->getBoolAttribute("switchOn", true) ? 1 : 0;
    macroSwitch = xml->getIntAttribute("macroSwitch", -1);
  }
}

void SwitchNode::process() {
  bool actualOn = macroSwitch != -1 && macros[(size_t)macroSwitch] != nullptr
                      ? (macros[(size_t)macroSwitch]->load() >= 0.5f)
                      : (switchOn != 0);

  auto it = inputSequences.find(0);
  NoteSequence inSeq =
      (it != inputSequences.end()) ? it->second : NoteSequence();

  if (actualOn) {
    outputSequences[0] = inSeq;
  } else {
    outputSequences[0] = NoteSequence();
  }

  auto conn = connections.find(0);
  if (conn != connections.end()) {
    for (const auto &c : conn->second) {
      c.targetNode->setInputSequence(c.targetInputPort, outputSequences[0]);
    }
  }
}
