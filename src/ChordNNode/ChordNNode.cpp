#include "ChordNNode.h"
#include "../LayoutParser.h"
#include "BinaryData.h"
#include <algorithm>
#include <set>

// --- ChordNNode Impl

ChordNNode::ChordNNode(std::array<std::atomic<float> *, 32> &inMacros)
    : macros(inMacros) {}

NodeLayout ChordNNode::getLayout() const {
  auto layout = LayoutParser::parseFromJSON(BinaryData::ChordNNode_json,
                                            BinaryData::ChordNNode_jsonSize);

  // Bind runtime pointers by matching element labels
  for (auto &el : layout.elements) {
   if (el.label == "nValue") {
      el.valueRef = const_cast<int *>(&nValue);
      el.macroIndexRef = const_cast<int *>(&macroNValue);
    }
  }

  return layout;
}

void ChordNNode::saveNodeState(juce::XmlElement *xml) {
  if (xml != nullptr) {
    xml->setAttribute("nValue", nValue);
    xml->setAttribute("macroNValue", macroNValue);
  }
}

void ChordNNode::loadNodeState(juce::XmlElement *xml) {
  if (xml != nullptr) {
    nValue = xml->getIntAttribute("nValue", 2);
    macroNValue = xml->getIntAttribute("macroNValue", -1);
  }
}

void ChordNNode::process() {
  int actualNValue =
      macroNValue != -1 && macros[(size_t)macroNValue] != nullptr
          ? 1 + (int)std::round(macros[(size_t)macroNValue]->load() * 15.0f)
          : nValue;

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
      auto cmb = [&](auto &self, std::vector<HeldNote> cur, size_t sI) -> void {
        if (cur.size() == n) {
          sortedSeq.push_back(cur);
          return;
        }
        if (sI >= uniqueNotes.size())
          return;
        for (size_t i = sI; i < uniqueNotes.size(); ++i) {
          if (cur.size() + (uniqueNotes.size() - i) < n)
            break;

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
