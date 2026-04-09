#include "MidiInNode.h"

#include <algorithm>
#include <cmath>

#include "../LayoutParser.h"
#include "BinaryData.h"

// --- MidiInNode Impl

MidiInNode::MidiInNode(NoteExpressionManager &handler)
    : noteExpressionManager(handler) {
  addMacroParam(&macroChannelFilter);
}

NodeLayout MidiInNode::getLayout() const {
  auto layout = LayoutParser::parseFromJSON(BinaryData::MidiInNode_json,
                                            BinaryData::MidiInNode_jsonSize);

  // Bind runtime pointers by matching element labels
  for (auto &el : layout.elements) {
    if (el.label == "channelFilter") {
      el.valueRef = const_cast<int *>(&channelFilter);
      el.macroParamRef = const_cast<MacroParam *>(&macroChannelFilter);
    }
  }

  return layout;
}

void MidiInNode::saveNodeState(juce::XmlElement *xml) {
  if (xml) {
    xml->setAttribute("channelFilter", channelFilter);
    saveMacroBindings(xml);
  }
}

void MidiInNode::loadNodeState(juce::XmlElement *xml) {
  if (xml) {
    channelFilter = xml->getIntAttribute("channelFilter", 0);
    if (xml->hasAttribute("macroChannelFilter")) {
      int m = xml->getIntAttribute("macroChannelFilter", -1);
      if (m != -1)
        macroChannelFilter.bindings.push_back({m, 1.0f});
    }
    loadMacroBindings(xml);
  }
}

void MidiInNode::process() {
  // MPE mode is now a global setting controlled from SettingsPanel.
  // Channel filtering is only applied when in legacy (non-MPE) mode.
  bool isLegacy = noteExpressionManager.isLegacyModeEnabled();

  int actualChannel = 0;

  if (isLegacy) {
    // Only apply channel filtering if MPE is disabled
    actualChannel = resolveMacroInt(macroChannelFilter, channelFilter, 0, 16);
  }

  auto heldNotes = noteExpressionManager.getHeldNotes(actualChannel);
  NoteSequence seq;
  for (const auto &note : heldNotes) {
    seq.push_back({note});
  }
  outputSequences[0] = seq;
}
