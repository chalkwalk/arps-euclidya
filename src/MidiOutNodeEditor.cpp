#include "MacroMappingMenu.h"
#include "MidiOutNode.h"
#include "SharedMacroUI.h"

class MidiOutNodeEditor : public juce::Component {
public:
  MidiOutNodeEditor(MidiOutNode &node,
                    juce::AudioProcessorValueTreeState &apvts)
      : midiOutNode(node) {
    // Setup simple UI for Pattern and Rhythm
    auto setupSlider = [this, &apvts](
                           CustomMacroSlider &slider, juce::Label &label,
                           int &nodeValueRef, int &nodeMacroRef,
                           std::unique_ptr<MacroAttachment> &attachment,
                           const juce::String &text, int minVal, int maxVal) {
      slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
      slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
      slider.setRange(minVal, maxVal, 1);
      slider.setValue(nodeValueRef);
      addAndMakeVisible(slider);

      label.setText(text, juce::dontSendNotification);
      label.setJustificationType(juce::Justification::centred);
      addAndMakeVisible(label);

      slider.onValueChange = [&slider, &nodeValueRef]() {
        nodeValueRef = (int)slider.getValue();
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

      slider.onRightClick = [&slider, &nodeMacroRef, &attachment, &apvts,
                             updateSliderVisibility]() {
        MacroMappingMenu::showMenu(
            &slider, nodeMacroRef,
            [&nodeMacroRef, &attachment, &apvts, &slider,
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
            });
      };

      // Initialize tooltip and attachment if previously mapped
      if (nodeMacroRef != -1) {
        slider.setTooltip("Mapped to Macro " + juce::String(nodeMacroRef + 1));
        attachment = std::make_unique<MacroAttachment>(
            apvts, "macro_" + juce::String(nodeMacroRef + 1), slider);
      }
      updateSliderVisibility(nodeMacroRef);
    };

    setupSlider(pSteps, lPSteps, midiOutNode.pSteps, midiOutNode.macroPSteps,
                aPSteps, "P Steps", 1, 32);
    setupSlider(pBeats, lPBeats, midiOutNode.pBeats, midiOutNode.macroPBeats,
                aPBeats, "P Beats", 1, 32);
    setupSlider(pOffset, lPOffset, midiOutNode.pOffset,
                midiOutNode.macroPOffset, aPOffset, "P Offset", 0, 32);

    setupSlider(rSteps, lRSteps, midiOutNode.rSteps, midiOutNode.macroRSteps,
                aRSteps, "R Steps", 1, 32);
    setupSlider(rBeats, lRBeats, midiOutNode.rBeats, midiOutNode.macroRBeats,
                aRBeats, "R Beats", 1, 32);
    setupSlider(rOffset, lROffset, midiOutNode.rOffset,
                midiOutNode.macroROffset, aROffset, "R Offset", 0, 32);

    setSize(400, 150);
  }

  void resized() override {
    auto bounds = getLocalBounds();
    auto topRow = bounds.removeFromTop(getHeight() / 2);

    int w = getWidth() / 3;

    auto setParamBounds = [](juce::Rectangle<int> &b, CustomMacroSlider &s,
                             juce::Label &l) {
      auto bCopy = b;
      l.setBounds(bCopy.removeFromBottom(20));
      // Make slider square to force drawing as a proper rotary knob
      int size = std::min(bCopy.getWidth(), bCopy.getHeight());
      s.setBounds(bCopy.withSizeKeepingCentre(size, size));
    };

    auto b1 = topRow.removeFromLeft(w);
    setParamBounds(b1, pSteps, lPSteps);
    auto b2 = topRow.removeFromLeft(w);
    setParamBounds(b2, pBeats, lPBeats);
    auto b3 = topRow.removeFromLeft(w);
    setParamBounds(b3, pOffset, lPOffset);

    auto b4 = bounds.removeFromLeft(w);
    setParamBounds(b4, rSteps, lRSteps);
    auto b5 = bounds.removeFromLeft(w);
    setParamBounds(b5, rBeats, lRBeats);
    auto b6 = bounds.removeFromLeft(w);
    setParamBounds(b6, rOffset, lROffset);
  }

private:
  MidiOutNode &midiOutNode;

  CustomMacroSlider pSteps, pBeats, pOffset;
  CustomMacroSlider rSteps, rBeats, rOffset;
  juce::Label lPSteps, lPBeats, lPOffset;
  juce::Label lRSteps, lRBeats, lROffset;

  std::unique_ptr<MacroAttachment> aPSteps, aPBeats, aPOffset;
  std::unique_ptr<MacroAttachment> aRSteps, aRBeats, aROffset;
};

std::unique_ptr<juce::Component>
MidiOutNode::createEditorComponent(juce::AudioProcessorValueTreeState &apvts) {
  return std::make_unique<MidiOutNodeEditor>(*this, apvts);
}
