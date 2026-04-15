#include "TransposeNode.h"

#include <algorithm>

#include "../LayoutParser.h"
#include "BinaryData.h"

// --- TransposeNode Impl

NodeLayout TransposeNode::getLayout() const {
  auto layout = LayoutParser::parseFromJSON(BinaryData::TransposeNode_json,
                                            BinaryData::TransposeNode_jsonSize);

  // Bind runtime pointers by matching element labels
  for (auto &el : layout.elements) {
    if (el.label == "semitones") {
      el.valueRef = const_cast<int *>(&semitones);
      el.macroParamRef = const_cast<MacroParam *>(&macroSemitones);
    }
  }

  return layout;
}

void TransposeNode::saveNodeState(juce::XmlElement *xml) {
  if (xml != nullptr) {
    xml->setAttribute("semitones", semitones);
    saveMacroBindings(xml);
  }
}

void TransposeNode::loadNodeState(juce::XmlElement *xml) {
  if (xml != nullptr) {
    semitones = xml->getIntAttribute("semitones", 0);
    // Backward compat for old saving format
    if (xml->hasAttribute("macroSemitones")) {
      int m = xml->getIntAttribute("macroSemitones", -1);
      if (m != -1)
        macroSemitones.bindings.push_back({m, 1.0f});
    }
    loadMacroBindings(xml);
  }
}

void TransposeNode::process() {
  int actualSemitones = resolveMacroOffset(macroSemitones, semitones, 24);

  auto it = inputSequences.find(0);
  if (it == inputSequences.end() || it->second.empty() ||
      actualSemitones == 0) {
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
          transposed.noteNumber += actualSemitones;
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
