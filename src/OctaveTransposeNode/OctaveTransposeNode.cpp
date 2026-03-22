#include "OctaveTransposeNode.h"
#include "../LayoutParser.h"
#include "BinaryData.h"
#include "../MacroMappingMenu.h"
#include <algorithm>

// --- OctaveTransposeNode Impl

OctaveTransposeNode::OctaveTransposeNode(
    std::array<std::atomic<float> *, 32> &inMacros)
    : macros(inMacros) {}

NodeLayout OctaveTransposeNode::getLayout() const {
  auto layout = LayoutParser::parseFromJSON(BinaryData::OctaveTransposeNode_json,
                                            BinaryData::OctaveTransposeNode_jsonSize);

  // Bind runtime pointers by matching element labels
  for (auto &el : layout.elements) {
   if (el.label == "octaves") {
      el.valueRef = const_cast<int *>(&octaves);
      el.macroIndexRef = const_cast<int *>(&macroOctaves);
    }
  }

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
