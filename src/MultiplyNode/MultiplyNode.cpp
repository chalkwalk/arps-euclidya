#include "MultiplyNode.h"
#include "../MacroMappingMenu.h"
#include "../SharedMacroUI.h"

void MultiplyNode::process() {
  auto it = inputSequences.find(0);
  if (it == inputSequences.end() || it->second.empty()) {
    outputSequences[0] = {};
    return;
  }

  int actualN =
      macroRepeatCount != -1 && macros[(size_t)macroRepeatCount] != nullptr
          ? 1 + (int)std::round(macros[(size_t)macroRepeatCount]->load() *
                                15.0f)
          : repeatCount;
  actualN = std::clamp(actualN, 1, 16);

  const auto &seq = it->second;
  NoteSequence result;
  result.reserve(seq.size() * (size_t)actualN);

  for (const auto &step : seq) {
    for (int r = 0; r < actualN; ++r) {
      result.push_back(step);
    }
  }

  outputSequences[0] = result;
}

void MultiplyNode::saveNodeState(juce::XmlElement *xml) {
  if (xml) {
    xml->setAttribute("repeatCount", repeatCount);
    xml->setAttribute("macroRepeatCount", macroRepeatCount);
  }
}

void MultiplyNode::loadNodeState(juce::XmlElement *xml) {
  if (xml) {
    repeatCount = xml->getIntAttribute("repeatCount", 2);
    macroRepeatCount = xml->getIntAttribute("macroRepeatCount", -1);
  }
}

NodeLayout MultiplyNode::getLayout() const {
  NodeLayout layout;
  layout.gridWidth = 1;
  layout.gridHeight = 1;

  UIElement label;
  label.type = UIElementType::Label;
  label.label = "MULTIPLY";
  label.gridBounds = {0, 0, 3, 1};
  layout.elements.push_back(label);

  UIElement slider;
  slider.type = UIElementType::RotarySlider;
  slider.minValue = 1;
  slider.maxValue = 16;
  slider.valueRef = const_cast<int *>(&repeatCount);
  slider.macroIndexRef = const_cast<int *>(&macroRepeatCount);
  slider.gridBounds = {0, 1, 3, 2};
  layout.elements.push_back(slider);

  return layout;
}
