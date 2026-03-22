#include "UnfoldNode.h"
#include "../LayoutParser.h"
#include "BinaryData.h"
#include <algorithm>

// --- UnfoldNode Impl

UnfoldNode::UnfoldNode() {}

NodeLayout UnfoldNode::getLayout() const {
  auto layout = LayoutParser::parseFromJSON(BinaryData::UnfoldNode_json,
                                            BinaryData::UnfoldNode_jsonSize);

  // Bind runtime pointers by matching element labels
  for (auto &el : layout.elements) {
   if (el.label == "ordering") {
      el.valueRef = const_cast<int *>(&ordering);
    }
  }

  return layout;
}

void UnfoldNode::saveNodeState(juce::XmlElement *xml) {
  if (xml != nullptr) {
    xml->setAttribute("ordering", ordering);
  }
}

void UnfoldNode::loadNodeState(juce::XmlElement *xml) {
  if (xml != nullptr) {
    ordering = xml->getIntAttribute("ordering", 0);
  }
}

void UnfoldNode::process() {
  auto it = inputSequences.find(0);
  if (it == inputSequences.end() || it->second.empty()) {
    outputSequences[0] = NoteSequence();
  } else {
    NoteSequence outSeq;

    for (const auto &step : it->second) {
      if (step.empty()) {
        outSeq.push_back({}); // preserve rest
        continue;
      }

      std::vector<HeldNote> currentStepNotes = step;

      if (ordering == 0) {
        // Ascending
        std::sort(currentStepNotes.begin(), currentStepNotes.end(),
                  [](const HeldNote &a, const HeldNote &b) {
                    return a.noteNumber < b.noteNumber;
                  });
      } else {
        // Descending
        std::sort(currentStepNotes.begin(), currentStepNotes.end(),
                  [](const HeldNote &a, const HeldNote &b) {
                    return a.noteNumber > b.noteNumber;
                  });
      }

      for (const auto &note : currentStepNotes) {
        outSeq.push_back({note});
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
