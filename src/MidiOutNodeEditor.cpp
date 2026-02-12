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

    // --- Clock Division ---
    const char *divNames[] = {"4/1", "2/1", "1/1",  "1/2",
                              "1/4", "1/8", "1/16", "1/32"};
    for (int i = 0; i < 8; ++i) {
      clockDivBox.addItem(divNames[i], i + 1);
    }
    clockDivBox.setSelectedId(midiOutNode.clockDivisionIndex + 1,
                              juce::dontSendNotification);
    clockDivBox.onChange = [this]() {
      midiOutNode.clockDivisionIndex = clockDivBox.getSelectedId() - 1;
    };
    addAndMakeVisible(clockDivBox);

    clockDivLabel.setText("Clock Div", juce::dontSendNotification);
    clockDivLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(clockDivLabel);

    tripletToggle.setButtonText("Triplet");
    tripletToggle.setToggleState(midiOutNode.triplet,
                                 juce::dontSendNotification);
    tripletToggle.onClick = [this]() {
      midiOutNode.triplet = tripletToggle.getToggleState();
    };
    addAndMakeVisible(tripletToggle);

    // --- Sync & Reset controls ---
    syncModeBox.addItem("Clock Sync", 1);
    syncModeBox.addItem("Key Sync", 2);
    syncModeBox.setSelectedId(midiOutNode.transportSyncMode + 1,
                              juce::dontSendNotification);
    syncModeBox.onChange = [this]() {
      midiOutNode.transportSyncMode = syncModeBox.getSelectedId() - 1;
    };
    addAndMakeVisible(syncModeBox);

    syncModeLabel.setText("Transport", juce::dontSendNotification);
    syncModeLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(syncModeLabel);

    patternResetToggle.setButtonText("Reset Pattern");
    patternResetToggle.setToggleState(midiOutNode.patternResetOnRelease,
                                      juce::dontSendNotification);
    patternResetToggle.onClick = [this]() {
      midiOutNode.patternResetOnRelease = patternResetToggle.getToggleState();
    };
    addAndMakeVisible(patternResetToggle);

    rhythmResetToggle.setButtonText("Reset Rhythm");
    rhythmResetToggle.setToggleState(midiOutNode.rhythmResetOnRelease,
                                     juce::dontSendNotification);
    rhythmResetToggle.onClick = [this]() {
      midiOutNode.rhythmResetOnRelease = rhythmResetToggle.getToggleState();
    };
    addAndMakeVisible(rhythmResetToggle);

    // --- Output Channel ---
    for (int ch = 1; ch <= 16; ++ch) {
      outputChannelBox.addItem("Ch " + juce::String(ch), ch);
    }
    outputChannelBox.setSelectedId(midiOutNode.outputChannel,
                                   juce::dontSendNotification);
    outputChannelBox.onChange = [this]() {
      midiOutNode.outputChannel = outputChannelBox.getSelectedId();
    };
    addAndMakeVisible(outputChannelBox);

    outputChannelLabel.setText("Out Ch", juce::dontSendNotification);
    outputChannelLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(outputChannelLabel);

    setSize(400, 380);
  }

  void resized() override {
    auto bounds = getLocalBounds();
    // 4 rows: Pattern knobs, Rhythm knobs, Clock div, Sync/Reset
    int knobRowHeight = 100;
    int ctrlRowHeight = 40;

    auto row1 = bounds.removeFromTop(knobRowHeight);
    auto row2 = bounds.removeFromTop(knobRowHeight);
    auto row3 = bounds.removeFromTop(ctrlRowHeight);
    auto row4 = bounds.removeFromTop(ctrlRowHeight);
    auto row5 = bounds.removeFromTop(ctrlRowHeight);

    int w = getWidth() / 3;

    auto setParamBounds = [](juce::Rectangle<int> &b, CustomMacroSlider &s,
                             juce::Label &l) {
      auto bCopy = b;
      l.setBounds(bCopy.removeFromBottom(20));
      int size = std::min(bCopy.getWidth(), bCopy.getHeight());
      s.setBounds(bCopy.withSizeKeepingCentre(size, size));
    };

    // Row 1: Pattern knobs
    auto b1 = row1.removeFromLeft(w);
    setParamBounds(b1, pSteps, lPSteps);
    auto b2 = row1.removeFromLeft(w);
    setParamBounds(b2, pBeats, lPBeats);
    auto b3 = row1.removeFromLeft(w);
    setParamBounds(b3, pOffset, lPOffset);

    // Row 2: Rhythm knobs
    auto b4 = row2.removeFromLeft(w);
    setParamBounds(b4, rSteps, lRSteps);
    auto b5 = row2.removeFromLeft(w);
    setParamBounds(b5, rBeats, lRBeats);
    auto b6 = row2.removeFromLeft(w);
    setParamBounds(b6, rOffset, lROffset);

    // Row 3: Clock division + Triplet
    auto r3a = row3.removeFromLeft(w);
    clockDivLabel.setBounds(r3a.removeFromLeft(r3a.getWidth() / 3));
    clockDivBox.setBounds(r3a.reduced(2));

    auto r3b = row3.removeFromLeft(w);
    tripletToggle.setBounds(r3b.reduced(4));

    // Row 4: Sync mode + toggles
    auto r4a = row4.removeFromLeft(w);
    syncModeLabel.setBounds(r4a.removeFromLeft(r4a.getWidth() / 3));
    syncModeBox.setBounds(r4a.reduced(2));

    auto r4b = row4.removeFromLeft(w);
    patternResetToggle.setBounds(r4b.reduced(4));

    auto r4c = row4.removeFromLeft(w);
    rhythmResetToggle.setBounds(r4c.reduced(4));

    // Row 5: Output channel
    auto r5a = row5.removeFromLeft(w);
    outputChannelLabel.setBounds(r5a.removeFromLeft(r5a.getWidth() / 3));
    outputChannelBox.setBounds(r5a.reduced(2));
  }

private:
  MidiOutNode &midiOutNode;

  CustomMacroSlider pSteps, pBeats, pOffset;
  CustomMacroSlider rSteps, rBeats, rOffset;
  juce::Label lPSteps, lPBeats, lPOffset;
  juce::Label lRSteps, lRBeats, lROffset;

  std::unique_ptr<MacroAttachment> aPSteps, aPBeats, aPOffset;
  std::unique_ptr<MacroAttachment> aRSteps, aRBeats, aROffset;

  // Clock division
  juce::ComboBox clockDivBox;
  juce::Label clockDivLabel;
  juce::ToggleButton tripletToggle;

  // Sync & Reset
  juce::ComboBox syncModeBox;
  juce::Label syncModeLabel;
  juce::ToggleButton patternResetToggle;
  juce::ToggleButton rhythmResetToggle;

  // Output Channel
  juce::ComboBox outputChannelBox;
  juce::Label outputChannelLabel;
};

std::unique_ptr<juce::Component>
MidiOutNode::createEditorComponent(juce::AudioProcessorValueTreeState &apvts) {
  return std::make_unique<MidiOutNodeEditor>(*this, apvts);
}
