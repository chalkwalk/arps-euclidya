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
    } else if (el.label == "mpeEnabled") {
      el.valueRef = const_cast<int *>(&mpeEnabled);
    }
  }

  return layout;
}

void MidiInNode::saveNodeState(juce::XmlElement *xml) {
  if (xml) {
    xml->setAttribute("channelFilter", channelFilter);
    xml->setAttribute("macroChannelFilter", macroChannelFilter);
    xml->setAttribute("mpeEnabled", mpeEnabled);
  }
}

void MidiInNode::loadNodeState(juce::XmlElement *xml) {
  if (xml) {
    channelFilter = xml->getIntAttribute("channelFilter", 0);
    macroChannelFilter = xml->getIntAttribute("macroChannelFilter", -1);

    // Fallback for old patches that saved "legacyMode"
    if (xml->hasAttribute("mpeEnabled")) {
      mpeEnabled = xml->getBoolAttribute("mpeEnabled", false) ? 1 : 0;
    } else if (xml->hasAttribute("legacyMode")) {
      mpeEnabled = xml->getBoolAttribute("legacyMode", false) ? 0 : 1;
    }
  }
}

void MidiInNode::process() {
  // NoteExpressionManager's legacy mode means "MPE is off". So we invert mpeEnabled.
  bool wantLegacy = (mpeEnabled == 0);
  if (noteExpressionManager.isLegacyModeEnabled() != wantLegacy) {
    noteExpressionManager.setLegacyMode(wantLegacy);
  }

  int actualChannel = 0;

  if (mpeEnabled == 0) {
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

  auto conn = connections.find(0);
  if (conn != connections.end()) {
    for (const auto &c : conn->second) {
      c.targetNode->setInputSequence(c.targetInputPort, outputSequences[0]);
    }
  }
}
