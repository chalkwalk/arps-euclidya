#include "MacroMappingMenu.h"
#include "MidiOutNode.h"
#include "SharedMacroUI.h"

class MidiOutNodeEditor : public juce::Component, private juce::Timer {
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

    // --- Cycle Length Display ---
    cycleLengthLabel.setJustificationType(juce::Justification::centredLeft);
    cycleLengthLabel.setFont(juce::Font(12.0f));
    cycleLengthLabel.setColour(juce::Label::textColourId,
                               juce::Colours::lightgrey);
    addAndMakeVisible(cycleLengthLabel);
    updateCycleLabel();
    startTimerHz(2); // Update 2x per second

    setSize(300, 380);
  }

  ~MidiOutNodeEditor() override { stopTimer(); }

  void timerCallback() override { updateCycleLabel(); }

  void updateCycleLabel() {
    cycleLengthLabel.setText("Cycle: " + midiOutNode.getCycleLengthInfo(),
                             juce::dontSendNotification);
  }

  void resized() override {
    auto bounds = getLocalBounds().reduced(8);
    int knobRowHeight = 70;
    int ctrlRowHeight = 28;
    int margin = 4;

    auto setParamBounds = [](juce::Rectangle<int> &b, CustomMacroSlider &s,
                             juce::Label &l) {
      auto bCopy = b;
      l.setBounds(bCopy.removeFromTop(16));
      int size = std::min(bCopy.getWidth(), bCopy.getHeight());
      s.setBounds(bCopy.withSizeKeepingCentre(size, size));
    };

    // Row 1: Pattern knobs
    auto r1 = bounds.removeFromTop(knobRowHeight);
    int w3 = r1.getWidth() / 3;
    auto b1 = r1.removeFromLeft(w3);
    setParamBounds(b1, pSteps, lPSteps);
    auto b2 = r1.removeFromLeft(w3);
    setParamBounds(b2, pBeats, lPBeats);
    auto b3 = r1;
    setParamBounds(b3, pOffset, lPOffset);

    bounds.removeFromTop(margin);

    // Row 2: Rhythm knobs
    auto r2 = bounds.removeFromTop(knobRowHeight);
    auto b4 = r2.removeFromLeft(w3);
    setParamBounds(b4, rSteps, lRSteps);
    auto b5 = r2.removeFromLeft(w3);
    setParamBounds(b5, rBeats, lRBeats);
    auto b6 = r2;
    setParamBounds(b6, rOffset, lROffset);

    bounds.removeFromTop(margin * 2);

    auto arrangeLabeledControl = [](juce::Rectangle<int> &row, juce::Label &lbl,
                                    juce::Component &comp, int lblWidth) {
      lbl.setBounds(row.removeFromLeft(lblWidth));
      comp.setBounds(row.reduced(2));
    };

    // Row 3: Clock division + Triplet
    auto r3 = bounds.removeFromTop(ctrlRowHeight);
    arrangeLabeledControl(r3, clockDivLabel, clockDivBox, 70);
    tripletToggle.setBounds(r3.removeFromRight(80));

    bounds.removeFromTop(margin);

    // Row 4: Sync Mode
    auto r4 = bounds.removeFromTop(ctrlRowHeight);
    arrangeLabeledControl(r4, syncModeLabel, syncModeBox, 70);

    bounds.removeFromTop(margin);

    // Row 5: Resets
    auto r5 = bounds.removeFromTop(ctrlRowHeight);
    patternResetToggle.setBounds(r5.removeFromLeft(r5.getWidth() / 2));
    rhythmResetToggle.setBounds(r5);

    bounds.removeFromTop(margin);

    // Row 6: Output Channel
    auto r6 = bounds.removeFromTop(ctrlRowHeight);
    arrangeLabeledControl(r6, outputChannelLabel, outputChannelBox, 70);

    bounds.removeFromTop(margin * 2);

    // Row 7: Cycle Info
    cycleLengthLabel.setBounds(bounds.removeFromTop(ctrlRowHeight));
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

  // Cycle length display
  juce::Label cycleLengthLabel;
};

std::unique_ptr<juce::Component>
MidiOutNode::createEditorComponent(juce::AudioProcessorValueTreeState &apvts) {
  return std::make_unique<MidiOutNodeEditor>(*this, apvts);
}
