#include "FoldNode.h"

#include <algorithm>

#include "../LayoutParser.h"
#include "BinaryData.h"

// --- FoldNode Impl

NodeLayout FoldNode::getLayout() const {
  auto layout = LayoutParser::parseFromJSON(BinaryData::FoldNode_json,
                                            BinaryData::FoldNode_jsonSize);

  // Bind runtime pointers by matching element labels
  for (auto &el : layout.elements) {
    if (el.label == "nValue") {
      el.valueRef = const_cast<int *>(&nValue);
      el.macroParamRef = const_cast<MacroParam *>(&macroNValue);
    } else if (el.label == "mode") {
      el.valueRef = const_cast<int *>(&mode);
    }
  }

  return layout;
}

void FoldNode::saveNodeState(juce::XmlElement *xml) {
  if (xml != nullptr) {
    xml->setAttribute("nValue", nValue);
    saveMacroBindings(xml);
    xml->setAttribute("mode", mode);
  }
}

void FoldNode::loadNodeState(juce::XmlElement *xml) {
  if (xml != nullptr) {
    nValue = xml->getIntAttribute("nValue", 2);
    if (xml->hasAttribute("macroNValue")) {
      int m = xml->getIntAttribute("macroNValue", -1);
      if (m != -1)
        macroNValue.bindings.push_back({m, 1.0f});
    }
    loadMacroBindings(xml);
    mode = xml->getIntAttribute("mode", 0);
  }
}

void FoldNode::process() {
  int actualNValue = resolveMacroInt(macroNValue, nValue, 0, 16);
  actualNValue = std::max(1, actualNValue);

  auto it = inputSequences.find(0);
  if (it == inputSequences.end() || it->second.empty() || actualNValue <= 1) {
    // If N is 1, folding does nothing, just pass through
    outputSequences[0] =
        it != inputSequences.end() ? it->second : NoteSequence();
  } else {
    NoteSequence outSeq;

    // Sort/dedup helpers for folded steps (note-order; non-note events sort low).
    auto sortByPitch = [](EventStep &s) {
      std::sort(s.begin(), s.end(),
                [](const SequenceEvent &a, const SequenceEvent &b) {
                  const auto *na = asNote(a);
                  const auto *nb = asNote(b);
                  return (na ? na->noteNumber : 0) < (nb ? nb->noteNumber : 0);
                });
    };
    auto dedupByPitch = [](EventStep &s) {
      auto last = std::unique(s.begin(), s.end(),
                              [](const SequenceEvent &a, const SequenceEvent &b) {
                                const auto *na = asNote(a);
                                const auto *nb = asNote(b);
                                return na && nb &&
                                       na->noteNumber == nb->noteNumber;
                              });
      s.erase(last, s.end());
    };

    if (mode == 0) {  // Chunked
      EventStep currentAggregatedStep;
      int itemsInCurrentStep = 0;

      for (const auto &step : it->second) {
        if (step.empty()) {
          continue;  // Skip resting steps during fold? Or fold them? Let's skip
        }
        // for now to compact notes.

        for (const auto &ev : step) {
          currentAggregatedStep.push_back(ev);
        }
        itemsInCurrentStep++;

        if (itemsInCurrentStep >= actualNValue) {
          sortByPitch(currentAggregatedStep);
          dedupByPitch(currentAggregatedStep);
          outSeq.push_back(currentAggregatedStep);
          currentAggregatedStep.clear();
          itemsInCurrentStep = 0;
        }
      }

      // Push any remaining aggregated notes as the final step
      if (!currentAggregatedStep.empty()) {
        sortByPitch(currentAggregatedStep);
        dedupByPitch(currentAggregatedStep);
        outSeq.push_back(currentAggregatedStep);
      }
    } else {  // Rolling
      const auto &inSeq = it->second;
      int seqSize = (int)inSeq.size();

      for (int i = 0; i < seqSize; ++i) {
        EventStep rollingStep;

        for (int j = 0; j < actualNValue; ++j) {
          const auto &sourceStep = inSeq[(size_t)((i + j) % seqSize)];
          for (const auto &ev : sourceStep) {
            rollingStep.push_back(ev);
          }
        }

        if (!rollingStep.empty()) {
          sortByPitch(rollingStep);
          dedupByPitch(rollingStep);
          outSeq.push_back(rollingStep);
        } else {
          outSeq.emplace_back();
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
