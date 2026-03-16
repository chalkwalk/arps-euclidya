#include "MultiplyNode.h"
#include "MacroMappingMenu.h"
#include "SharedMacroUI.h"

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

// --- Editor ---
class MultiplyNodeEditor : public juce::Component {
public:
  MultiplyNodeEditor(MultiplyNode &node,
                     juce::AudioProcessorValueTreeState &apvts)
      : multiplyNode(node) {

    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setRange(1, 16, 1);
    slider.setValue(node.repeatCount);
    slider.onValueChange = [this]() {
      multiplyNode.repeatCount = (int)slider.getValue();
      if (multiplyNode.onNodeDirtied)
        multiplyNode.onNodeDirtied();
    };
    addAndMakeVisible(slider);

    label.setText("MULTIPLY", juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(label);

    // Right-click macro mapping
    slider.onRightClick = [this, &node, &apvts]() {
      MacroMappingMenu::showMenu(
          &slider, node.macroRepeatCount,
          [this, &node, &apvts](int macroIndex) {
            node.macroRepeatCount = macroIndex;
            if (macroIndex == -1) {
              attachment.reset();
              slider.setTooltip("");
              slider.removeColour(juce::Slider::rotarySliderFillColourId);
              slider.removeColour(juce::Slider::rotarySliderOutlineColourId);
            } else {
              attachment = std::make_unique<MacroAttachment>(
                  apvts, "macro_" + juce::String(macroIndex + 1), slider);
              slider.setTooltip("Mapped to Macro " +
                                juce::String(macroIndex + 1));
              slider.setColour(juce::Slider::rotarySliderFillColourId,
                               juce::Colours::orange);
              slider.setColour(juce::Slider::rotarySliderOutlineColourId,
                               juce::Colours::orange.withAlpha(0.3f));
            }
          });
    };

    if (node.macroRepeatCount != -1) {
      attachment = std::make_unique<MacroAttachment>(
          apvts, "macro_" + juce::String(node.macroRepeatCount + 1), slider);
      slider.setTooltip("Mapped to Macro " +
                        juce::String(node.macroRepeatCount + 1));
      slider.setColour(juce::Slider::rotarySliderFillColourId,
                       juce::Colours::orange);
      slider.setColour(juce::Slider::rotarySliderOutlineColourId,
                       juce::Colours::orange.withAlpha(0.3f));
    }

    setSize(100, 100);
  }

  void resized() override {
    auto b = getLocalBounds().reduced(2);
    label.setBounds(b.removeFromTop(16));
    slider.setBounds(b);
  }

private:
  MultiplyNode &multiplyNode;
  CustomMacroSlider slider;
  juce::Label label;
  std::unique_ptr<MacroAttachment> attachment;
};

std::unique_ptr<juce::Component>
MultiplyNode::createEditorComponent(juce::AudioProcessorValueTreeState &apvts) {
  return std::make_unique<MultiplyNodeEditor>(*this, apvts);
}
