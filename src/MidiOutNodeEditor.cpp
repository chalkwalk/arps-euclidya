#include "EuclideanVisualizer.h"
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
      slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
      slider.setRange(minVal, maxVal, 1);

      if (isBipolar) {
        slider.getProperties().set("bipolar", true);
      }

      slider.setValue(nodeValueRef, juce::dontSendNotification);
      addAndMakeVisible(slider);

      label.setText(text + ": " + juce::String(nodeValueRef),
                    juce::dontSendNotification);
      label.setJustificationType(juce::Justification::centred);
      label.setFont(juce::Font(13.0f, juce::Font::bold));
      addAndMakeVisible(label);

      slider.onValueChange = [this, &slider, &label, &nodeValueRef, text]() {
        nodeValueRef = (int)slider.getValue();
        label.setText(text + ": " + juce::String(nodeValueRef),
                      juce::dontSendNotification);
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

    // --- Visualization ---
    addAndMakeVisible(visualizer);

    patternTitle.setText("PATTERN", juce::dontSendNotification);
    patternTitle.setJustificationType(juce::Justification::centred);
    patternTitle.setFont(juce::Font(14.0f, juce::Font::bold));
    patternTitle.setColour(juce::Label::textColourId, juce::Colours::magenta);
    addAndMakeVisible(patternTitle);

    rhythmTitle.setText("RHYTHM", juce::dontSendNotification);
    rhythmTitle.setJustificationType(juce::Justification::centred);
    rhythmTitle.setFont(juce::Font(14.0f, juce::Font::bold));
    rhythmTitle.setColour(juce::Label::textColourId, juce::Colours::cyan);
    addAndMakeVisible(rhythmTitle);

    // --- Cycle Length Display ---
    cycleLengthLabel.setJustificationType(juce::Justification::centredLeft);
    cycleLengthLabel.setFont(juce::Font(12.0f));
    cycleLengthLabel.setColour(juce::Label::textColourId,
                               juce::Colours::lightgrey);
    addAndMakeVisible(cycleLengthLabel);
    updateCycleLabel();
    startTimerHz(30); // Higher rate for smooth playhead
    setSize(500, 320);
  }

  ~MidiOutNodeEditor() override { stopTimer(); }

  void timerCallback() override {
    updateCycleLabel();
    updateSliderRanges();
    visualizer.update(midiOutNode.getPattern(), midiOutNode.getPatternIndex(),
                      midiOutNode.getRhythm(), midiOutNode.getRhythmIndex());
  }

  void updateSliderRanges() {
    // Pattern
    pBeats.setRange(1, std::max(1, midiOutNode.pSteps), 1);
    pBeats.setValue(midiOutNode.pBeats, juce::dontSendNotification);
    lPSteps.setText("P Steps: " + juce::String(midiOutNode.pSteps),
                    juce::dontSendNotification);
    lPBeats.setText("P Beats: " + juce::String(midiOutNode.pBeats),
                    juce::dontSendNotification);

    int halfP = (midiOutNode.pSteps + 1) / 2;
    pOffset.setRange(-halfP, halfP, 1);
    pOffset.setValue(midiOutNode.pOffset, juce::dontSendNotification);
    lPOffset.setText("P Offset: " + juce::String(midiOutNode.pOffset),
                     juce::dontSendNotification);

    // Rhythm
    rBeats.setRange(1, std::max(1, midiOutNode.rSteps), 1);
    rBeats.setValue(midiOutNode.rBeats, juce::dontSendNotification);
    lRSteps.setText("R Steps: " + juce::String(midiOutNode.rSteps),
                    juce::dontSendNotification);
    lRBeats.setText("R Beats: " + juce::String(midiOutNode.rBeats),
                    juce::dontSendNotification);

    int halfR = (midiOutNode.rSteps + 1) / 2;
    rOffset.setRange(-halfR, halfR, 1);
    rOffset.setValue(midiOutNode.rOffset, juce::dontSendNotification);
    lROffset.setText("R Offset: " + juce::String(midiOutNode.rOffset),
                     juce::dontSendNotification);
  }

  void updateCycleLabel() {
    cycleLengthLabel.setText("Cycle: " + midiOutNode.getCycleLengthInfo(),
                             juce::dontSendNotification);
  }

  void resized() override {
    auto bounds = getLocalBounds().reduced(8);

    // Bottom area for Sync/Out controls and Cycle Info
    auto bottomArea = bounds.removeFromBottom(85);
    auto cycleBounds = bottomArea.removeFromBottom(20);
    cycleLengthLabel.setBounds(cycleBounds);

    int ctrlRowHeight = 24;
    auto row2 = bottomArea.removeFromBottom(ctrlRowHeight);
    auto row1 = bottomArea.removeFromTop(ctrlRowHeight);

    int thirdW = row1.getWidth() / 3;

    auto arrangeLabeledControl = [](juce::Rectangle<int> row, juce::Label &lbl,
                                    juce::Component &comp, int lblWidth) {
      lbl.setBounds(row.removeFromLeft(lblWidth));
      comp.setBounds(row.reduced(2, 1));
    };

    // Row 1: Clock Div, Triplet, Output Channel
    auto r1a = row1.removeFromLeft(thirdW);
    arrangeLabeledControl(r1a, clockDivLabel, clockDivBox, 60);
    auto r1b = row1.removeFromLeft(thirdW);
    tripletToggle.setBounds(r1b.reduced(10, 0));
    auto r1c = row1;
    arrangeLabeledControl(r1c, outputChannelLabel, outputChannelBox, 45);

    // Row 2: Sync Mode, Resets
    auto r2a = row2.removeFromLeft(thirdW);
    arrangeLabeledControl(r2a, syncModeLabel, syncModeToggle, 60);
    auto r2b = row2.removeFromLeft(thirdW);
    patternResetToggle.setBounds(r2b.reduced(4, 0));
    auto r2c = row2;
    rhythmResetToggle.setBounds(r2c.reduced(4, 0));

    // Main Columns: Pattern (Left), Visualizer (Center), Rhythm (Right)
    bounds.removeFromBottom(10);      // Spacing
    int colW = bounds.getWidth() / 4; // 1:2:1 ratio

    auto leftCol = bounds.removeFromLeft(colW);
    auto rightCol = bounds.removeFromRight(colW);
    visualizer.setBounds(bounds);

    auto setColumnBounds = [](juce::Rectangle<int> col, juce::Label &title,
                              CustomMacroSlider &s1, juce::Label &l1,
                              CustomMacroSlider &s2, juce::Label &l2,
                              CustomMacroSlider &s3, juce::Label &l3) {
      title.setBounds(col.removeFromTop(20));
      int kh = col.getHeight() / 3;

      auto b1 = col.removeFromTop(kh);
      l1.setBounds(b1.removeFromBottom(14));
      s1.setBounds(b1.reduced(2));

      auto b2 = col.removeFromTop(kh);
      l2.setBounds(b2.removeFromBottom(14));
      s2.setBounds(b2.reduced(2));

      auto b3 = col;
      l3.setBounds(b3.removeFromBottom(14));
      s3.setBounds(b3.reduced(2));
    };

    setColumnBounds(leftCol, patternTitle, pSteps, lPSteps, pBeats, lPBeats,
                    pOffset, lPOffset);
    setColumnBounds(rightCol, rhythmTitle, rSteps, lRSteps, rBeats, lRBeats,
                    rOffset, lROffset);
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

  // Visualization
  EuclideanVisualizer visualizer;
  juce::Label patternTitle, rhythmTitle;

  // Cycle length display
  juce::Label cycleLengthLabel;
};

std::unique_ptr<juce::Component>
MidiOutNode::createEditorComponent(juce::AudioProcessorValueTreeState &apvts) {
  return std::make_unique<MidiOutNodeEditor>(*this, apvts);
}
