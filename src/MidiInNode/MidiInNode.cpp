#include "MidiInNode.h"
#include <algorithm>
#include <cmath>

// --- MidiInNode Impl

MidiInNode::MidiInNode(MidiHandler &handler,
                       std::array<std::atomic<float> *, 32> macrosArray)
    : midiHandler(handler), macros(macrosArray) {}

NodeLayout MidiInNode::getLayout() const {
  NodeLayout layout;
  layout.gridWidth = 1;
  layout.gridHeight = 1;

  UIElement slider;
  slider.type = UIElementType::RotarySlider;
  slider.label = "CHANNEL";
  slider.valueRef = const_cast<int *>(&channelFilter);
  slider.macroIndexRef = const_cast<int *>(&macroChannelFilter);
  slider.minValue = 0;
  slider.maxValue = 16;
  slider.gridBounds = {0, 0, 3, 2};
  layout.elements.push_back(slider);

  UIElement toggle;
  toggle.type = UIElementType::Toggle;
  toggle.label = "LEGACY";
  toggle.valueRef = const_cast<int *>(&legacyMode);
  toggle.gridBounds = {0, 2, 3, 1};
  layout.elements.push_back(toggle);

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

  // Wrap the held notes in a single-step sequence
  outputSequences[0] = {midiHandler.getHeldNotes(actualChannel)};

  auto conn = connections.find(0);
  if (conn != connections.end()) {
    for (const auto &c : conn->second) {
      c.targetNode->setInputSequence(c.targetInputPort, outputSequences[0]);
    }
  }
}
