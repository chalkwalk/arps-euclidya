#include "FoldNode.h"

#include <algorithm>

#include "../LayoutParser.h"
#include "../MacroMappingMenu.h"
#include "BinaryData.h"

// --- FoldNode Impl

FoldNode::FoldNode(std::array<std::atomic<float> *, 32> &inMacros)
    : macros(inMacros) {}

NodeLayout FoldNode::getLayout() const {
  auto layout = LayoutParser::parseFromJSON(BinaryData::FoldNode_json,
                                            BinaryData::FoldNode_jsonSize);

  // Bind runtime pointers by matching element labels
  for (auto &el : layout.elements) {
    if (el.label == "nValue") {
      el.valueRef = const_cast<int *>(&nValue);
      el.macroIndexRef = const_cast<int *>(&macroNValue);
    } else if (el.label == "mode") {
      el.valueRef = const_cast<int *>(&mode);
    }
  }

  return layout;
}

void FoldNode::saveNodeState(juce::XmlElement *xml) {
  if (xml != nullptr) {
    xml->setAttribute("nValue", nValue);
    xml->setAttribute("macroNValue", macroNValue);
    xml->setAttribute("mode", mode);
  }
}

void FoldNode::loadNodeState(juce::XmlElement *xml) {
  if (xml != nullptr) {
    nValue = xml->getIntAttribute("nValue", 2);
    macroNValue = xml->getIntAttribute("macroNValue", -1);
    mode = xml->getIntAttribute("mode", 0);
  }
}

void FoldNode::process() {
  int actualNValue =
      macroNValue != -1 && macros[(size_t)macroNValue] != nullptr
          ? 1 + (int)std::round(macros[(size_t)macroNValue]->load() * 15.0f)
          : nValue;

  auto it = inputSequences.find(0);
  if (it == inputSequences.end() || it->second.empty() || actualNValue <= 1) {
    // If N is 1, folding does nothing, just pass through
    outputSequences[0] =
        it != inputSequences.end() ? it->second : NoteSequence();
  } else {
    NoteSequence outSeq;

    if (mode == 0) {  // Chunked
      std::vector<HeldNote> currentAggregatedStep;
      int itemsInCurrentStep = 0;

      for (const auto &step : it->second) {
        if (step.empty())
          continue;  // Skip resting steps during fold? Or fold them? Let's skip
                     // for now to compact notes.

        for (const auto &note : step) {
          currentAggregatedStep.push_back(note);
        }
        itemsInCurrentStep++;

        if (itemsInCurrentStep >= actualNValue) {
          // Sort and dedup before pushing the folded step
          std::sort(currentAggregatedStep.begin(), currentAggregatedStep.end(),
                    [](const HeldNote &a, const HeldNote &b) {
                      return a.noteNumber < b.noteNumber;
                    });

          auto last = std::unique(currentAggregatedStep.begin(),
                                  currentAggregatedStep.end(),
                                  [](const HeldNote &a, const HeldNote &b) {
                                    return a.noteNumber == b.noteNumber;
                                  });
          currentAggregatedStep.erase(last, currentAggregatedStep.end());

          outSeq.push_back(currentAggregatedStep);
          currentAggregatedStep.clear();
          itemsInCurrentStep = 0;
        }
      }

      // Push any remaining aggregated notes as the final step
      if (!currentAggregatedStep.empty()) {
        std::sort(currentAggregatedStep.begin(), currentAggregatedStep.end(),
                  [](const HeldNote &a, const HeldNote &b) {
                    return a.noteNumber < b.noteNumber;
                  });

        auto last = std::unique(currentAggregatedStep.begin(),
                                currentAggregatedStep.end(),
                                [](const HeldNote &a, const HeldNote &b) {
                                  return a.noteNumber == b.noteNumber;
                                });
        currentAggregatedStep.erase(last, currentAggregatedStep.end());

        outSeq.push_back(currentAggregatedStep);
      }
    } else {  // Rolling
      const auto &inSeq = it->second;
      int seqSize = (int)inSeq.size();

      for (int i = 0; i < seqSize; ++i) {
        std::vector<HeldNote> rollingStep;

        for (int j = 0; j < actualNValue; ++j) {
          const auto &sourceStep = inSeq[(size_t)((i + j) % seqSize)];
          for (const auto &note : sourceStep) {
            rollingStep.push_back(note);
          }
        }

        if (!rollingStep.empty()) {
          std::sort(rollingStep.begin(), rollingStep.end(),
                    [](const HeldNote &a, const HeldNote &b) {
                      return a.noteNumber < b.noteNumber;
                    });

          auto last = std::unique(rollingStep.begin(), rollingStep.end(),
                                  [](const HeldNote &a, const HeldNote &b) {
                                    return a.noteNumber == b.noteNumber;
                                  });
          rollingStep.erase(last, rollingStep.end());
          outSeq.push_back(rollingStep);
        } else {
          outSeq.push_back({});
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
