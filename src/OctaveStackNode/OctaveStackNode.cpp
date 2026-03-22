#include "OctaveStackNode.h"
#include "../MacroMappingMenu.h"
#include <algorithm>
#include <set>

// --- OctaveStackNode Impl

OctaveStackNode::OctaveStackNode(std::array<std::atomic<float> *, 32> &inMacros)
    : macros(inMacros) {}

NodeLayout OctaveStackNode::getLayout() const {
  NodeLayout layout;
  layout.gridWidth = 1;
  layout.gridHeight = 1;

  UIElement label;
  label.type = UIElementType::Label;
  label.label = "Octaves";
  label.gridBounds = {0, 0, 2, 1};
  layout.elements.push_back(label);

  UIElement slider;
  slider.type = UIElementType::RotarySlider;
  slider.minValue = 1;
  slider.maxValue = 4;
  slider.valueRef = const_cast<int *>(&octaves);
  slider.macroIndexRef = const_cast<int *>(&macroOctaves);
  slider.gridBounds = {0, 1, 2, 1};
  layout.elements.push_back(slider);

  UIElement uniqueToggle;
  uniqueToggle.type = UIElementType::Toggle;
  uniqueToggle.label = "Unique";
  uniqueToggle.valueRef = const_cast<int *>(&uniqueOnly);
  uniqueToggle.gridBounds = {0, 2, 2, 1};
  layout.elements.push_back(uniqueToggle);

  return layout;
}

void OctaveStackNode::saveNodeState(juce::XmlElement *xml) {
  if (xml != nullptr) {
    xml->setAttribute("octaves", octaves);
    xml->setAttribute("macroOctaves", macroOctaves);
    xml->setAttribute("uniqueOnly", uniqueOnly != 0);
  }
}

void OctaveStackNode::loadNodeState(juce::XmlElement *xml) {
  if (xml != nullptr) {
    octaves = xml->getIntAttribute("octaves", 1);
    macroOctaves = xml->getIntAttribute("macroOctaves", -1);
    uniqueOnly = xml->getBoolAttribute("uniqueOnly", true) ? 1 : 0;
  }
}

void OctaveStackNode::process() {
  int actualOctaves =
      macroOctaves != -1 && macros[macroOctaves] != nullptr
          ? 1 + (int)std::round(macros[macroOctaves]->load() * 3.0f)
          : octaves;

  auto it = inputSequences.find(0);
  if (it == inputSequences.end() || it->second.empty()) {
    outputSequences[0] = NoteSequence();
  } else {
    NoteSequence outSeq;
    std::set<int> seenNotes;

    for (int oct = 0; oct < actualOctaves; ++oct) {
      for (const auto &step : it->second) {
        std::vector<HeldNote> outStep;
        for (const auto &note : step) {
          HeldNote n = note;
          n.noteNumber += (oct * 12);

          if (n.noteNumber <= 127) {
            if (!uniqueOnly ||
                seenNotes.find(n.noteNumber) == seenNotes.end()) {
              outStep.push_back(n);
              if (uniqueOnly) {
                seenNotes.insert(n.noteNumber);
              }
            }
          }
        }
        if (!outStep.empty()) {
          outSeq.push_back(outStep);
        }
      }
    }

    outputSequences[0] = outSeq;
  }

  auto connIt = connections.find(0);
  if (connIt != connections.end()) {
    for (const auto &connection : connIt->second) {
      connection.targetNode->setInputSequence(connection.targetInputPort,
                                              outputSequences[0]);
    }
  }
}
