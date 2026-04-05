#include "OctaveStackNode.h"

#include <algorithm>
#include <set>

#include "../LayoutParser.h"
#include "../MacroMappingMenu.h"
#include "BinaryData.h"

// --- OctaveStackNode Impl

NodeLayout OctaveStackNode::getLayout() const {
  auto layout = LayoutParser::parseFromJSON(
      BinaryData::OctaveStackNode_json, BinaryData::OctaveStackNode_jsonSize);

  // Bind runtime pointers by matching element labels
  for (auto &el : layout.elements) {
    if (el.label == "octaves") {
      el.valueRef = const_cast<int *>(&octaves);
      el.macroParamRef = const_cast<MacroParam *>(&macroOctaves);
    } else if (el.label == "uniqueOnly") {
      el.valueRef = const_cast<int *>(&uniqueOnly);
    }
  }

  return layout;
}

void OctaveStackNode::saveNodeState(juce::XmlElement *xml) {
  if (xml != nullptr) {
    xml->setAttribute("octaves", octaves);
    saveMacroBindings(xml);
    xml->setAttribute("uniqueOnly", uniqueOnly != 0);
  }
}

void OctaveStackNode::loadNodeState(juce::XmlElement *xml) {
  if (xml != nullptr) {
    octaves = xml->getIntAttribute("octaves", 1);
    if (xml->hasAttribute("macroOctaves")) {
      int m = xml->getIntAttribute("macroOctaves", -1);
      if (m != -1)
        macroOctaves.bindings.push_back({m, 1.0f});
    }
    loadMacroBindings(xml);
    uniqueOnly = xml->getBoolAttribute("uniqueOnly", true) ? 1 : 0;
  }
}

void OctaveStackNode::process() {
  int actualOctaves = resolveMacroInt(macroOctaves, octaves, 0, 4);
  actualOctaves = std::max(1, actualOctaves);

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
