#include "ChordNNode.h"

#include <algorithm>
#include <set>

#include "../LayoutParser.h"
#include "BinaryData.h"

// --- ChordNNode Impl

NodeLayout ChordNNode::getLayout() const {
  auto layout = LayoutParser::parseFromJSON(BinaryData::ChordNNode_json,
                                            BinaryData::ChordNNode_jsonSize);

  // Bind runtime pointers by matching element labels
  for (auto &el : layout.elements) {
    if (el.label == "nValue") {
      el.valueRef = const_cast<int *>(&nValue);
      el.macroParamRef = const_cast<MacroParam *>(&macroNValue);
    }
  }

  return layout;
}

void ChordNNode::saveNodeState(juce::XmlElement *xml) {
  if (xml != nullptr) {
    xml->setAttribute("nValue", nValue);
    saveMacroBindings(xml);
  }
}

void ChordNNode::loadNodeState(juce::XmlElement *xml) {
  if (xml != nullptr) {
    nValue = xml->getIntAttribute("nValue", 2);
    if (xml->hasAttribute("macroNValue")) {
      int m = xml->getIntAttribute("macroNValue", -1);
      if (m != -1)
        macroNValue.bindings.push_back({m, 1.0f});
    }
    loadMacroBindings(xml);
  }
}

void ChordNNode::process() {
  int actualNValue = resolveMacroInt(macroNValue, nValue, 0, 16);
  actualNValue = std::max(1, actualNValue);

  auto it = inputSequences.find(0);
  if (it == inputSequences.end() || it->second.empty() || actualNValue <= 0) {
    outputSequences[0] = NoteSequence();
  } else {
    // 1. Extract all unique notes from the incoming sequence into a pool
    std::set<int> uniquePitches;
    std::vector<HeldNote> uniqueNotes;

    for (const auto &step : it->second) {
      for (const auto &note : step) {
        if (uniquePitches.find(note.noteNumber) == uniquePitches.end()) {
          uniquePitches.insert(note.noteNumber);
          uniqueNotes.push_back(note);
        }
      }
    }

    // 2. Sort the unique notes ascending by pitch
    std::sort(uniqueNotes.begin(), uniqueNotes.end(),
              [](const HeldNote &a, const HeldNote &b) {
                return a.noteNumber < b.noteNumber;
              });

    size_t n = std::min((size_t)actualNValue, uniqueNotes.size());
    NoteSequence sortedSeq;

    // 3. Generate N-note combinations from the flat unique pool
    if (n > 0 && n <= uniqueNotes.size()) {
      auto cmb = [&](auto &self, const std::vector<HeldNote> &cur,
                     size_t sI) -> void {
        if (cur.size() == n) {
          sortedSeq.push_back(cur);
          return;
        }
        if (sI >= uniqueNotes.size()) {
          return;
        }
        for (size_t i = sI; i < uniqueNotes.size(); ++i) {
          if (cur.size() + (uniqueNotes.size() - i) < n) {
            break;
          }

          std::vector<HeldNote> nextCur = cur;
          nextCur.push_back(uniqueNotes[i]);

          self(self, nextCur, i + 1);
        }
      };
      cmb(cmb, std::vector<HeldNote>(), 0);
    }

    outputSequences[0] = sortedSeq;
  }

  auto connIt = connections.find(0);
  if (connIt != connections.end()) {
    for (const auto &connection : connIt->second) {
      connection.targetNode->setInputSequence(connection.targetInputPort,
                                              outputSequences[0]);
    }
  }
}
