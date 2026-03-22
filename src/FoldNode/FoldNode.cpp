#include "FoldNode.h"
#include "../MacroMappingMenu.h"
#include <algorithm>

// --- FoldNode Impl

FoldNode::FoldNode(std::array<std::atomic<float> *, 32> &inMacros)
    : macros(inMacros) {}

NodeLayout FoldNode::getLayout() const {
  NodeLayout layout;
  layout.gridWidth = 2;
  layout.gridHeight = 2;

  UIElement label;
  label.type = UIElementType::Label;
  label.label = "N Value";
  label.gridBounds = {0, 0, 6, 1};
  layout.elements.push_back(label);

  UIElement slider;
  slider.type = UIElementType::RotarySlider;
  slider.minValue = 1;
  slider.maxValue = 16;
  slider.valueRef = const_cast<int *>(&nValue);
  slider.macroIndexRef = const_cast<int *>(&macroNValue);
  slider.gridBounds = {1, 1, 4, 4};
  layout.elements.push_back(slider);

  return layout;
}

void FoldNode::saveNodeState(juce::XmlElement *xml) {
  if (xml != nullptr) {
    xml->setAttribute("nValue", nValue);
    xml->setAttribute("macroNValue", macroNValue);
  }
}

void FoldNode::loadNodeState(juce::XmlElement *xml) {
  if (xml != nullptr) {
    nValue = xml->getIntAttribute("nValue", 2);
    macroNValue = xml->getIntAttribute("macroNValue", -1);
  }
}

void FoldNode::process() {
  int actualNValue =
      macroNValue != -1 && macros[macroNValue] != nullptr
          ? 1 + (int)std::round(macros[macroNValue]->load() * 15.0f)
          : nValue;

  auto it = inputSequences.find(0);
  if (it == inputSequences.end() || it->second.empty() || actualNValue <= 1) {
    // If N is 1, folding does nothing, just pass through
    outputSequences[0] =
        it != inputSequences.end() ? it->second : NoteSequence();
  } else {
    NoteSequence outSeq;
    std::vector<HeldNote> currentAggregatedStep;
    int itemsInCurrentStep = 0;

    for (const auto &step : it->second) {
      if (step.empty())
        continue; // Skip resting steps during fold? Or fold them? Let's skip
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
