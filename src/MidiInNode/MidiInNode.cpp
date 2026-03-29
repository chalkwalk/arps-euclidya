#include "MidiInNode.h"

#include <algorithm>
#include <cmath>

#include "../LayoutParser.h"
#include "BinaryData.h"

// --- MidiInNode Impl

MidiInNode::MidiInNode(MidiHandler &handler,
                       std::array<std::atomic<float> *, 32> macrosArray)
    : midiHandler(handler), macros(macrosArray) {}

NodeLayout MidiInNode::getLayout() const {
  auto layout = LayoutParser::parseFromJSON(BinaryData::MidiInNode_json,
                                            BinaryData::MidiInNode_jsonSize);

  // Bind runtime pointers by matching element labels
  for (auto &el : layout.elements) {
    if (el.label == "channelFilter") {
      el.valueRef = const_cast<int *>(&channelFilter);
      el.macroIndexRef = const_cast<int *>(&macroChannelFilter);
    } else if (el.label == "legacyMode") {
      el.valueRef = const_cast<int *>(&legacyMode);
    }
  }

  return layout;
}

void MidiInNode::saveNodeState(juce::XmlElement *xml) {
  if (xml) {
    xml->setAttribute("channelFilter", channelFilter);
    xml->setAttribute("macroChannelFilter", macroChannelFilter);
    xml->setAttribute("legacyMode", legacyMode);
  }
}

void MidiInNode::loadNodeState(juce::XmlElement *xml) {
  if (xml) {
    channelFilter = xml->getIntAttribute("channelFilter", 0);
    macroChannelFilter = xml->getIntAttribute("macroChannelFilter", -1);
    legacyMode = xml->getBoolAttribute("legacyMode", false);
  }
}

void MidiInNode::process() {
  midiHandler.setLegacyMode(legacyMode != 0);

  int actualChannel =
      macroChannelFilter != -1 && macros[(size_t)macroChannelFilter] != nullptr
          ? (int)std::round(macros[(size_t)macroChannelFilter]->load() * 16.0f)
          : channelFilter;

  auto heldNotes = midiHandler.getHeldNotes(actualChannel);
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
