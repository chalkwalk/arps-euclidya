#include "TransposeNode.h"
#include <algorithm>

// --- TransposeNode Impl

TransposeNode::TransposeNode(std::array<std::atomic<float> *, 32> &inMacros)
    : macros(inMacros) {}

NodeLayout TransposeNode::getLayout() const {
  NodeLayout layout;
  layout.gridWidth = 1;
  layout.gridHeight = 1;

  UIElement slider;
  slider.type = UIElementType::RotarySlider;
  slider.label = "TRANSPOSE";
  slider.valueRef = const_cast<int *>(&semitones);
  slider.macroIndexRef = const_cast<int *>(&macroSemitones);
  slider.minValue = -24;
  slider.maxValue = 24;
  // Node is 1x1 major (100x100), body starts at PORT_MARGIN (14) and
  // HEADER_HEIGHT (28). A 3x2 sub-grid fits well.
  slider.gridBounds = {0, 0, 3, 2};
  layout.elements.push_back(slider);

  return layout;
}

void TransposeNode::saveNodeState(juce::XmlElement *xml) {
  if (xml != nullptr) {
    xml->setAttribute("semitones", semitones);
    xml->setAttribute("macroSemitones", macroSemitones);
  }
}

void TransposeNode::loadNodeState(juce::XmlElement *xml) {
  if (xml != nullptr) {
    semitones = xml->getIntAttribute("semitones", 0);
    macroSemitones = xml->getIntAttribute("macroSemitones", -1);
  }
}

void TransposeNode::process() {
  int actualSemitones =
      macroSemitones != -1 && macros[macroSemitones] != nullptr
          ? -24 + (int)std::round(macros[macroSemitones]->load() * 48.0f)
          : semitones;

  auto it = inputSequences.find(0);
  if (it == inputSequences.end() || it->second.empty() ||
      actualSemitones == 0) {
    // If no transposition needed, just pass through
    outputSequences[0] =
        it != inputSequences.end() ? it->second : NoteSequence();
  } else {
    NoteSequence outSeq;
    for (const auto &step : it->second) {
      std::vector<HeldNote> outStep;
      for (const auto &note : step) {
        HeldNote transposed = note;
        transposed.noteNumber += actualSemitones;
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
