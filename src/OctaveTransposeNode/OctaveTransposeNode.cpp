#include "OctaveTransposeNode.h"

#include <algorithm>

#include "../LayoutParser.h"
#include "BinaryData.h"

// --- OctaveTransposeNode Impl

NodeLayout OctaveTransposeNode::getLayout() const {
  auto layout =
      LayoutParser::parseFromJSON(BinaryData::OctaveTransposeNode_json,
                                  BinaryData::OctaveTransposeNode_jsonSize);

  // Bind runtime pointers by matching element labels
  for (auto &el : layout.elements) {
    if (el.label == "octaves") {
      el.valueRef = const_cast<int *>(&octaves);
      el.macroParamRef = const_cast<MacroParam *>(&macroOctaves);
    }
  }

  return layout;
}

void OctaveTransposeNode::saveNodeState(juce::XmlElement *xml) {
  if (xml != nullptr) {
    xml->setAttribute("octaves", octaves);
    saveMacroBindings(xml);
  }
}

void OctaveTransposeNode::loadNodeState(juce::XmlElement *xml) {
  if (xml != nullptr) {
    octaves = xml->getIntAttribute("octaves", 0);
    if (xml->hasAttribute("macroOctaves")) {
      int m = xml->getIntAttribute("macroOctaves", -1);
      if (m != -1)
        macroOctaves.bindings.push_back({m, 1.0f});
    }
    loadMacroBindings(xml);
  }
}

void OctaveTransposeNode::process() {
  int actualOctaves = resolveMacroOffset(macroOctaves, octaves, 4);

  auto it = inputSequences.find(0);
  if (it == inputSequences.end() || it->second.empty() || actualOctaves == 0) {
    // If no transposition needed, just pass through
    outputSequences[0] =
        it != inputSequences.end() ? it->second : NoteSequence();
  } else {
    NoteSequence outSeq;
    for (const auto &step : it->second) {
      EventStep outStep;
      for (const auto &ev : step) {
        if (const auto *note = asNote(ev)) {
          HeldNote transposed = *note;
          transposed.noteNumber += (actualOctaves * 12);
          if (transposed.noteNumber >= 0 && transposed.noteNumber <= 127) {
            outStep.push_back(transposed);
          }
        }
      }
      outSeq.push_back(outStep);  // preserve rests (empty steps)
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
