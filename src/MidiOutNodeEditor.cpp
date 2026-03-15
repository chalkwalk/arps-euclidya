#include "MacroMappingMenu.h"
#include "MidiOutNode.h"
#include "SharedMacroUI.h"

class MidiOutNodeEditor : public juce::Component, private juce::Timer {
public:
  MidiOutNodeEditor(MidiOutNode &node,
                    juce::AudioProcessorValueTreeState &apvts)
      : midiOutNode(node) {
    // Setup simple UI for Pattern and Rhythm
    auto setupSlider = [this,
                        &apvts](CustomMacroSlider &slider, juce::Label &label,
                                int &nodeValueRef, int &nodeMacroRef,
                                std::unique_ptr<MacroAttachment> &attachment,
                                const juce::String &text, int minVal,
                                int maxVal, bool isBipolar = false) {
      slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
      slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
      slider.setRange(minVal, maxVal, 1);

      if (isBipolar) {
        slider.getProperties().set("bipolar", true);
      }

      slider.setValue(nodeValueRef, juce::dontSendNotification);
      addAndMakeVisible(slider);

      label.setText(text, juce::dontSendNotification);
      label.setJustificationType(juce::Justification::centred);
      addAndMakeVisible(label);

      slider.onValueChange = [this, &slider, &nodeValueRef]() {
        nodeValueRef = (int)slider.getValue();
        midiOutNode.clampParameters();
        updateSliderRanges();
        updateCycleLabel();
        if (midiOutNode.onNodeDirtied)
          midiOutNode.onNodeDirtied();
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
                midiOutNode.macroPOffset, aPOffset, "P Offset", -16, 16, true);

    setupSlider(rSteps, lRSteps, midiOutNode.rSteps, midiOutNode.macroRSteps,
                aRSteps, "R Steps", 1, 32);
    setupSlider(rBeats, lRBeats, midiOutNode.rBeats, midiOutNode.macroRBeats,
                aRBeats, "R Beats", 1, 32);
    setupSlider(rOffset, lROffset, midiOutNode.rOffset,
                midiOutNode.macroROffset, aROffset, "R Offset", -16, 16, true);

    updateSliderRanges();

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
      if (midiOutNode.onNodeDirtied)
        midiOutNode.onNodeDirtied();
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
      if (midiOutNode.onNodeDirtied)
        midiOutNode.onNodeDirtied();
    };
    addAndMakeVisible(tripletToggle);

    // --- Sync & Reset controls ---
    syncModeToggle.setButtonText(
        midiOutNode.transportSyncMode == 0 ? "Clock Sync" : "Key Sync");
    syncModeToggle.setToggleState(midiOutNode.transportSyncMode != 0,
                                  juce::dontSendNotification);
    syncModeToggle.setClickingTogglesState(true);
    syncModeToggle.onClick = [this]() {
      midiOutNode.transportSyncMode = syncModeToggle.getToggleState() ? 1 : 0;
      syncModeToggle.setButtonText(
          midiOutNode.transportSyncMode == 0 ? "Clock Sync" : "Key Sync");
      if (midiOutNode.onNodeDirtied)
        midiOutNode.onNodeDirtied();
    };
    addAndMakeVisible(syncModeToggle);

    syncModeLabel.setText("Transport", juce::dontSendNotification);
    syncModeLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(syncModeLabel);

    patternResetToggle.setButtonText("Reset Pattern");
    patternResetToggle.setToggleState(midiOutNode.patternResetOnRelease,
                                      juce::dontSendNotification);
    patternResetToggle.onClick = [this]() {
      midiOutNode.patternResetOnRelease = patternResetToggle.getToggleState();
      if (midiOutNode.onNodeDirtied)
        midiOutNode.onNodeDirtied();
    };
    addAndMakeVisible(patternResetToggle);

    rhythmResetToggle.setButtonText("Reset Rhythm");
    rhythmResetToggle.setToggleState(midiOutNode.rhythmResetOnRelease,
                                     juce::dontSendNotification);
    rhythmResetToggle.onClick = [this]() {
      midiOutNode.rhythmResetOnRelease = rhythmResetToggle.getToggleState();
      if (midiOutNode.onNodeDirtied)
        midiOutNode.onNodeDirtied();
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
      if (midiOutNode.onNodeDirtied)
        midiOutNode.onNodeDirtied();
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
    setSize(390, 190);
  }

  ~MidiOutNodeEditor() override { stopTimer(); }

  void timerCallback() override {
    updateCycleLabel();
    updateSliderRanges();
  }

  void updateSliderRanges() {
    // Pattern
    pBeats.setRange(1, std::max(1, midiOutNode.pSteps), 1);
    pBeats.setValue(midiOutNode.pBeats, juce::dontSendNotification);

    int halfP = (midiOutNode.pSteps + 1) / 2;
    pOffset.setRange(-halfP, halfP, 1);
    pOffset.setValue(midiOutNode.pOffset, juce::dontSendNotification);

    // Rhythm
    rBeats.setRange(1, std::max(1, midiOutNode.rSteps), 1);
    rBeats.setValue(midiOutNode.rBeats, juce::dontSendNotification);

    int halfR = (midiOutNode.rSteps + 1) / 2;
    rOffset.setRange(-halfR, halfR, 1);
    rOffset.setValue(midiOutNode.rOffset, juce::dontSendNotification);
  }

  void updateCycleLabel() {
    cycleLengthLabel.setText("Cycle: " + midiOutNode.getCycleLengthInfo(),
                             juce::dontSendNotification);
  }

  void resized() override {
    auto bounds = getLocalBounds().reduced(6);
    int knobRowHeight = 65;
    int ctrlRowHeight = 24;
    int margin = 4;

    auto setParamBounds = [](juce::Rectangle<int> &b, CustomMacroSlider &s,
                             juce::Label &l) {
      auto bCopy = b;
      l.setBounds(bCopy.removeFromTop(16));
      int size = std::min(bCopy.getWidth(), bCopy.getHeight());
      s.setBounds(bCopy.withSizeKeepingCentre(size, size));
    };

    // Row 1: All 6 Knobs
    auto r1 = bounds.removeFromTop(knobRowHeight);
    int w6 = r1.getWidth() / 6;
    auto b1 = r1.removeFromLeft(w6);
    setParamBounds(b1, pSteps, lPSteps);
    auto b2 = r1.removeFromLeft(w6);
    setParamBounds(b2, pBeats, lPBeats);
    auto b3 = r1.removeFromLeft(w6);
    setParamBounds(b3, pOffset, lPOffset);
    auto b4 = r1.removeFromLeft(w6);
    setParamBounds(b4, rSteps, lRSteps);
    auto b5 = r1.removeFromLeft(w6);
    setParamBounds(b5, rBeats, lRBeats);
    auto b6 = r1;
    setParamBounds(b6, rOffset, lROffset);

    bounds.removeFromTop(margin);

    auto arrangeLabeledControl = [](juce::Rectangle<int> &row, juce::Label &lbl,
                                    juce::Component &comp, int lblWidth) {
      lbl.setBounds(row.removeFromLeft(lblWidth));
      comp.setBounds(row.reduced(2, 1));
    };

    // Row 2: Clock division + Triplet + Out Ch
    auto r2 = bounds.removeFromTop(ctrlRowHeight);
    int thirdW = r2.getWidth() / 3;
    auto r2a = r2.removeFromLeft(thirdW);
    arrangeLabeledControl(r2a, clockDivLabel, clockDivBox, 60);
    auto r2b = r2.removeFromLeft(thirdW);
    tripletToggle.setBounds(r2b.reduced(10, 0));
    auto r2c = r2;
    arrangeLabeledControl(r2c, outputChannelLabel, outputChannelBox, 45);

    bounds.removeFromTop(margin);

    // Row 3: Sync Mode + Resets
    auto r3 = bounds.removeFromTop(ctrlRowHeight);
    auto r3a = r3.removeFromLeft(thirdW);
    arrangeLabeledControl(r3a, syncModeLabel, syncModeToggle, 60);
    auto r3b = r3.removeFromLeft(thirdW);
    patternResetToggle.setBounds(r3b.reduced(10, 0));
    auto r3c = r3;
    rhythmResetToggle.setBounds(r3c.reduced(10, 0));

    bounds.removeFromTop(margin);

    // Row 4: Cycle Info
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
  juce::ToggleButton syncModeToggle;
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
