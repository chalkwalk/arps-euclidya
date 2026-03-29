#include "UnfoldNode.h"

#include <algorithm>

#include "../LayoutParser.h"
#include "BinaryData.h"

// --- UnfoldNode Impl

UnfoldNode::UnfoldNode() = default;

NodeLayout UnfoldNode::getLayout() const {
  auto layout = LayoutParser::parseFromJSON(BinaryData::UnfoldNode_json,
                                            BinaryData::UnfoldNode_jsonSize);

  // Bind runtime pointers by matching element labels
  for (auto &el : layout.elements) {
    if (el.label == "ordering") {
      el.valueRef = const_cast<int *>(&ordering);
    } else if (el.label == "fixedWidth") {
      el.valueRef = const_cast<int *>(&fixedWidth);
    } else if (el.label == "noteBias") {
      el.valueRef = const_cast<int *>(&noteBias);
    }
  }

  return layout;
}

void UnfoldNode::saveNodeState(juce::XmlElement *xml) {
  if (xml != nullptr) {
    xml->setAttribute("ordering", ordering);
    xml->setAttribute("fixedWidth", fixedWidth);
    xml->setAttribute("noteBias", noteBias);
  }
}

void UnfoldNode::loadNodeState(juce::XmlElement *xml) {
  if (xml != nullptr) {
    ordering = xml->getIntAttribute("ordering", 0);
    fixedWidth = xml->getIntAttribute("fixedWidth", 0);
    noteBias = xml->getIntAttribute("noteBias", 0);
  }
}

void UnfoldNode::process() {
  auto it = inputSequences.find(0);
  if (it == inputSequences.end() || it->second.empty()) {
    outputSequences[0] = NoteSequence();
  } else {
    const NoteSequence &inSeq = it->second;
    NoteSequence outSeq;

    // Helper: sort a chord's notes by ordering
    auto sortNotes = [this](std::vector<HeldNote> notes) {
      if (ordering == 0) {
        std::sort(notes.begin(), notes.end(),
                  [](const HeldNote &a, const HeldNote &b) {
                    return a.noteNumber < b.noteNumber;
                  });
      } else {
        std::sort(notes.begin(), notes.end(),
                  [](const HeldNote &a, const HeldNote &b) {
                    return a.noteNumber > b.noteNumber;
                  });
      }
      return notes;
    };

    if (fixedWidth == 0) {
      // --- Variable width (original behaviour) ---
      for (const auto &step : inSeq) {
        if (step.empty()) {
          outSeq.emplace_back();  // preserve rest
          continue;
        }
        auto sorted = sortNotes(step);
        for (const auto &note : sorted) {
          outSeq.push_back({note});
        }
      }
    } else {
      // --- Fixed width ---
      // 1. Find the max chord size across all non-rest steps
      int maxChordSize = 0;
      for (const auto &step : inSeq) {
        maxChordSize = std::max(maxChordSize, (int)step.size());
      }
      if (maxChordSize == 0) {
        maxChordSize = 1;  // guard
      }

      // 2. For each input step, emit exactly maxChordSize output steps
      for (const auto &step : inSeq) {
        if (step.empty()) {
          // A rest: emit maxChordSize rest slots
          for (int s = 0; s < maxChordSize; ++s)
            outSeq.emplace_back();
          continue;
        }

        auto sorted = sortNotes(step);

        // If more notes than slots, trim by bias
        if ((int)sorted.size() > maxChordSize) {
          if (noteBias == 0) {
            // Bottom: keep first maxChordSize (lowest when asc, highest when
            // desc)
            sorted.resize((size_t)maxChordSize);
          } else if (noteBias == 2) {
            // Top: keep last maxChordSize
            sorted.erase(sorted.begin(),
                         sorted.begin() + ((int)sorted.size() - maxChordSize));
          } else {
            // Middle: drop equally from each end
            int excess = (int)sorted.size() - maxChordSize;
            int dropFront = excess / 2;
            int dropBack = excess - dropFront;
            sorted.erase(sorted.end() - dropBack, sorted.end());
            sorted.erase(sorted.begin(), sorted.begin() + dropFront);
          }
        }

        // Emit real notes
        for (const auto &note : sorted) {
          outSeq.push_back({note});
        }
        // Pad with rests to fill the slot
        int padCount = maxChordSize - (int)sorted.size();
        for (int p = 0; p < padCount; ++p)
          outSeq.emplace_back();
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
