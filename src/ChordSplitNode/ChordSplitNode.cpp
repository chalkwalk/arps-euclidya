#include "ChordSplitNode.h"
#include "../LayoutParser.h"
#include "BinaryData.h"
#include <algorithm>

void ChordSplitNode::process() {
  auto it = inputSequences.find(0);
  if (it == inputSequences.end() || it->second.empty()) {
    outputSequences[0] = {};
    outputSequences[1] = {};
    return;
  }

  const auto &seq = it->second;
  NoteSequence out0, out1;

  for (const auto &step : seq) {
    if (step.empty()) {
      // Rest: both outputs get a rest
      out0.push_back({});
      out1.push_back({});
      continue;
    }

    if (step.size() == 1) {
      // Single note: it goes to one output, the other gets a rest
      if (splitMode == 0) {
        // Top mode: the single note IS the highest, so it goes to out0 (Top)
        out0.push_back(step);
        out1.push_back({});
      } else {
        // Bottom mode: the single note IS the lowest, so it goes to out1
        // (Bottom)
        out0.push_back({});
        out1.push_back(step);
      }
      continue;
    }

    // Multiple notes: sort by pitch to find highest/lowest
    auto sorted = step;
    std::sort(sorted.begin(), sorted.end(),
              [](const HeldNote &a, const HeldNote &b) {
                return a.noteNumber < b.noteNumber;
              });

    if (splitMode == 0) {
      // Top mode: highest note → out0 (Top), rest → out1 (Bottom)
      std::vector<HeldNote> lower(sorted.begin(), sorted.end() - 1);
      std::vector<HeldNote> higher = {sorted.back()};
      out0.push_back(higher);
      out1.push_back(lower.empty() ? std::vector<HeldNote>{} : lower);
    } else {
      // Bottom mode: lowest note → out1 (Bottom), rest → out0 (Top)
      std::vector<HeldNote> lowest = {sorted.front()};
      std::vector<HeldNote> upper(sorted.begin() + 1, sorted.end());
      out0.push_back(upper.empty() ? std::vector<HeldNote>{} : upper);
      out1.push_back(lowest);
    }
  }

  outputSequences[0] = out0;
  outputSequences[1] = out1;
}

void ChordSplitNode::saveNodeState(juce::XmlElement *xml) {
  if (xml) {
    xml->setAttribute("splitMode", splitMode);
  }
}

void ChordSplitNode::loadNodeState(juce::XmlElement *xml) {
  if (xml) {
    splitMode = xml->getIntAttribute("splitMode", 0);
  }
}

NodeLayout ChordSplitNode::getLayout() const {
  auto layout = LayoutParser::parseFromJSON(BinaryData::ChordSplitNode_json,
                                            BinaryData::ChordSplitNode_jsonSize);

  // Bind runtime pointers by matching element labels
  for (auto &el : layout.elements) {
   if (el.label == "splitMode") {
      el.valueRef = const_cast<int *>(&splitMode);
    }
  }

  return layout;
}
