#include "MacroMappingMenu.h"
#include "MidiOutNode.h"

class CustomMacroSlider : public juce::Slider {
public:
  std::function<void()> onRightClick;

  void mouseDown(const juce::MouseEvent &e) override {
    if (e.mods.isPopupMenu()) {
      if (onRightClick)
        onRightClick();
    } else {
      juce::Slider::mouseDown(e);
    }
  }
};

class MidiOutNodeEditor : public juce::Component {
public:
  MidiOutNodeEditor(MidiOutNode &node,
                    juce::AudioProcessorValueTreeState &apvts)
      : midiOutNode(node) {
    // Setup simple UI for Pattern and Rhythm
    auto setupSlider =
        [this, &apvts](CustomMacroSlider &slider, juce::Label &label,
                       int &nodeValueRef, int &nodeMacroRef,
                       std::unique_ptr<
                           juce::AudioProcessorValueTreeState::SliderAttachment>
                           &attachment,
                       const juce::String &text) {
          slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
          slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
          slider.setRange(1, 32, 1);
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
                    attachment = std::make_unique<
                        juce::AudioProcessorValueTreeState::SliderAttachment>(
                        apvts, "macro_" + juce::String(macroIndex + 1), slider);
                    slider.setTooltip("Mapped to Macro " +
                                      juce::String(macroIndex + 1));
                  }
                  updateSliderVisibility(macroIndex);
                });
          };

          // Initialize tooltip and attachment if previously mapped
          if (nodeMacroRef != -1) {
            slider.setTooltip("Mapped to Macro " +
                              juce::String(nodeMacroRef + 1));
            attachment = std::make_unique<
                juce::AudioProcessorValueTreeState::SliderAttachment>(
                apvts, "macro_" + juce::String(nodeMacroRef + 1), slider);
          }
          updateSliderVisibility(nodeMacroRef);
        };

    setupSlider(pSteps, lPSteps, midiOutNode.pSteps, midiOutNode.macroPSteps,
                aPSteps, "P Steps");
    setupSlider(pBeats, lPBeats, midiOutNode.pBeats, midiOutNode.macroPBeats,
                aPBeats, "P Beats");
    setupSlider(pOffset, lPOffset, midiOutNode.pOffset,
                midiOutNode.macroPOffset, aPOffset, "P Offset");

    setupSlider(rSteps, lRSteps, midiOutNode.rSteps, midiOutNode.macroRSteps,
                aRSteps, "R Steps");
    setupSlider(rBeats, lRBeats, midiOutNode.rBeats, midiOutNode.macroRBeats,
                aRBeats, "R Beats");
    setupSlider(rOffset, lROffset, midiOutNode.rOffset,
                midiOutNode.macroROffset, aROffset, "R Offset");

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

  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> aPSteps,
      aPBeats, aPOffset;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> aRSteps,
      aRBeats, aROffset;
};

std::unique_ptr<juce::Component>
MidiOutNode::createEditorComponent(juce::AudioProcessorValueTreeState &apvts) {
  return std::make_unique<MidiOutNodeEditor>(*this, apvts);
}
