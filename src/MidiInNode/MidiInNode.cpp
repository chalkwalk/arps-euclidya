#include "MidiInNode.h"

#include <algorithm>
#include <cmath>

#include "../LayoutParser.h"
#include "BinaryData.h"

// --- MidiInNode Impl

MidiInNode::MidiInNode(NoteExpressionManager &handler,
                       std::array<std::atomic<float> *, 32> macrosArray)
    : noteExpressionManager(handler), macros(macrosArray) {}

NodeLayout MidiInNode::getLayout() const {
  auto layout = LayoutParser::parseFromJSON(BinaryData::MidiInNode_json,
                                            BinaryData::MidiInNode_jsonSize);

  // Bind runtime pointers by matching element labels
  for (auto &el : layout.elements) {
    if (el.label == "channelFilter") {
      el.valueRef = const_cast<int *>(&channelFilter);
      el.macroIndexRef = const_cast<int *>(&macroChannelFilter);
    }
  }

  return layout;
}

void MidiInNode::saveNodeState(juce::XmlElement *xml) {
  if (xml) {
    xml->setAttribute("channelFilter", channelFilter);
    xml->setAttribute("macroChannelFilter", macroChannelFilter);
  }
}

void MidiInNode::loadNodeState(juce::XmlElement *xml) {
  if (xml) {
    channelFilter = xml->getIntAttribute("channelFilter", 0);
    macroChannelFilter = xml->getIntAttribute("macroChannelFilter", -1);
  }
}

void MidiInNode::process() {
  // MPE mode is now a global setting controlled from SettingsPanel.
  // Channel filtering is only applied when in legacy (non-MPE) mode.
  bool isLegacy = noteExpressionManager.isLegacyModeEnabled();

  int actualChannel = 0;

  if (isLegacy) {
    // Only apply channel filtering if MPE is disabled
    actualChannel =
        macroChannelFilter != -1 &&
                macros[(size_t)macroChannelFilter] != nullptr
            ? (int)std::round(macros[(size_t)macroChannelFilter]->load() *
                              16.0f)
            : channelFilter;
  }

  auto heldNotes = noteExpressionManager.getHeldNotes(actualChannel);
  NoteSequence seq;
  for (const auto &note : heldNotes) {
    seq.push_back({note});
  }
  outputSequences[0] = seq;
}
