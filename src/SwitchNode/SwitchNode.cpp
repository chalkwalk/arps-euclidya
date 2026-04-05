#include "SwitchNode.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include "../LayoutParser.h"
#include "BinaryData.h"

// --- SwitchNode Impl

NodeLayout SwitchNode::getLayout() const {
  auto layout = LayoutParser::parseFromJSON(BinaryData::SwitchNode_json,
                                            BinaryData::SwitchNode_jsonSize);

  // Bind runtime pointers by matching element labels
  for (auto &el : layout.elements) {
    if (el.label == "switchOn") {
      el.valueRef = const_cast<int *>(&switchOn);
      el.macroParamRef = const_cast<MacroParam *>(&macroSwitch);
    }
  }

  return layout;
}

void SwitchNode::saveNodeState(juce::XmlElement *xml) {
  if (xml) {
    xml->setAttribute("switchOn", (bool)(switchOn != 0));
    saveMacroBindings(xml);
  }
}

void SwitchNode::loadNodeState(juce::XmlElement *xml) {
  if (xml) {
    switchOn = xml->getBoolAttribute("switchOn", true) ? 1 : 0;
    if (xml->hasAttribute("macroSwitch")) {
      int m = xml->getIntAttribute("macroSwitch", -1);
      if (m != -1)
        macroSwitch.bindings.push_back({m, 1.0f});
    }
    loadMacroBindings(xml);
  }
}

void SwitchNode::process() {
  bool actualOn = (resolveMacroInt(macroSwitch, switchOn, 0, 1) != 0);

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
