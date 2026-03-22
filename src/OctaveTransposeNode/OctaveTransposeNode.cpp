#include "OctaveTransposeNode.h"
#include "../MacroMappingMenu.h"
#include <algorithm>

// --- OctaveTransposeNode Impl

OctaveTransposeNode::OctaveTransposeNode(
    std::array<std::atomic<float> *, 32> &inMacros)
    : macros(inMacros) {}

NodeLayout OctaveTransposeNode::getLayout() const {
  NodeLayout layout;
  layout.gridWidth = 1;
  layout.gridHeight = 1;

  UIElement label;
  label.type = UIElementType::Label;
  label.label = "Octave";
  label.gridBounds = {0, 0, 3, 1};
  layout.elements.push_back(label);

  UIElement slider;
  slider.type = UIElementType::RotarySlider;
  slider.minValue = -4;
  slider.maxValue = 4;
  slider.valueRef = const_cast<int *>(&octaves);
  slider.macroIndexRef = const_cast<int *>(&macroOctaves);
  slider.gridBounds = {0, 1, 3, 2};
  layout.elements.push_back(slider);

  return layout;
}

void OctaveTransposeNode::saveNodeState(juce::XmlElement *xml) {
  if (xml != nullptr) {
    xml->setAttribute("octaves", octaves);
    xml->setAttribute("macroOctaves", macroOctaves);
  }
}

void OctaveTransposeNode::loadNodeState(juce::XmlElement *xml) {
  if (xml != nullptr) {
    octaves = xml->getIntAttribute("octaves", 0);
    macroOctaves = xml->getIntAttribute("macroOctaves", -1);
  }
}

void OctaveTransposeNode::process() {
  int actualOctaves =
      macroOctaves != -1 && macros[macroOctaves] != nullptr
          ? -4 + (int)std::round(macros[macroOctaves]->load() * 8.0f)
          : octaves;

  auto it = inputSequences.find(0);
  if (it == inputSequences.end() || it->second.empty() || actualOctaves == 0) {
    // If no transposition needed, just pass through
    outputSequences[0] =
        it != inputSequences.end() ? it->second : NoteSequence();
  } else {
    NoteSequence outSeq;
    for (const auto &step : it->second) {
      std::vector<HeldNote> outStep;
      for (const auto &note : step) {
        HeldNote transposed = note;
        transposed.noteNumber += (actualOctaves * 12);
        if (transposed.noteNumber >= 0 && transposed.noteNumber <= 127) {
          outStep.push_back(transposed);
        }
      }
      if (!outStep.empty()) {
        outSeq.push_back(outStep);
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
