#include "FoldNode.h"
#include "MacroMappingMenu.h"
#include <algorithm>

class FoldNodeEditor : public juce::Component {
public:
  FoldNodeEditor(FoldNode &node, juce::AudioProcessorValueTreeState &apvts)
      : foldNode(node) {

    auto setupSlider = [this, &apvts](
                           CustomMacroSlider &slider, juce::Label &label,
                           int &nodeValueRef, int &nodeMacroRef,
                           std::unique_ptr<MacroAttachment> &attachment,
                           const juce::String &labelText, int min, int max) {
      slider.setRange(min, max, 1);
      slider.setValue(nodeValueRef, juce::dontSendNotification);
      slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
      slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
      addAndMakeVisible(slider);

      label.setText(labelText, juce::dontSendNotification);
      label.setJustificationType(juce::Justification::centred);
      addAndMakeVisible(label);

      slider.onValueChange = [this, &slider, &nodeValueRef]() {
        nodeValueRef = (int)slider.getValue();
        if (foldNode.onNodeDirtied)
          foldNode.onNodeDirtied();
      };

      auto updateSliderVisibility = [&slider](int macro) {
        if (macro == -1) {
          slider.removeColour(juce::Slider::rotarySliderFillColourId);
          slider.removeColour(juce::Slider::rotarySliderOutlineColourId);
        } else {
          slider.setColour(juce::Slider::rotarySliderFillColourId,
                           juce::Colours::orange);
          slider.setColour(juce::Slider::rotarySliderOutlineColourId,
                           juce::Colours::orange.withAlpha(0.3f));
        }
      };

      updateSliderVisibility(nodeMacroRef);

      slider.onRightClick = [this, &slider, &nodeMacroRef, &attachment, &apvts,
                             updateSliderVisibility]() {
        MacroMappingMenu::showMenu(
            &slider, nodeMacroRef,
            [this, &nodeMacroRef, &attachment, &apvts, &slider,
             updateSliderVisibility](int macroIndex) {
              nodeMacroRef = macroIndex;
              if (macroIndex == -1) {
                attachment.reset();
                slider.setTooltip("");
              } else {
                attachment = std::make_unique<MacroAttachment>(
                    apvts, "macro_" + juce::String(macroIndex + 1), slider);
                slider.setTooltip("Mapped to Macro " +
                                  juce::String(macroIndex + 1));
              }
              updateSliderVisibility(macroIndex);
              if (foldNode.onMappingChanged)
                foldNode.onMappingChanged();
            });
      };

      // INIT
      if (nodeMacroRef != -1) {
        juce::String paramID = "macro_" + juce::String(nodeMacroRef + 1);
        attachment = std::make_unique<MacroAttachment>(apvts, paramID, slider);
      }
    };

    setupSlider(nValueSlider, nValueLabel, foldNode.nValue,
                foldNode.macroNValue, nValueAttachment, "N Value", 1, 16);

    setSize(400, 150);
  }

  void resized() override {
    auto bounds = getLocalBounds().reduced(10);
    nValueLabel.setBounds(bounds.removeFromTop(20));
    int size = std::min(bounds.getWidth(), bounds.getHeight());
    nValueSlider.setBounds(bounds.withSizeKeepingCentre(size, size));
  }

private:
  FoldNode &foldNode;
  CustomMacroSlider nValueSlider;
  juce::Label nValueLabel;
  std::unique_ptr<MacroAttachment> nValueAttachment;
};

// --- FoldNode Impl

FoldNode::FoldNode(std::array<std::atomic<float> *, 32> &inMacros)
    : macros(inMacros) {}

std::unique_ptr<juce::Component>
FoldNode::createEditorComponent(juce::AudioProcessorValueTreeState &apvts) {
  return std::make_unique<FoldNodeEditor>(*this, apvts);
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
