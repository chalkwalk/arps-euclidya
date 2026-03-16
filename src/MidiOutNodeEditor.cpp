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

      label.setText(text, juce::dontSendNotification);
      label.setJustificationType(juce::Justification::centred);
      label.setFont(juce::Font(11.0f));
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
    syncModeBox.addItem("Gestural", (int)MidiOutNode::SyncMode::Gestural + 1);
    syncModeBox.addItem("Synchronized",
                        (int)MidiOutNode::SyncMode::Synchronized + 1);
    syncModeBox.addItem("Deterministic",
                        (int)MidiOutNode::SyncMode::Deterministic + 1);
    syncModeBox.setSelectedId((int)midiOutNode.syncMode + 1,
                              juce::dontSendNotification);
    syncModeBox.onChange = [this]() {
      midiOutNode.syncMode =
          (MidiOutNode::SyncMode)(syncModeBox.getSelectedId() - 1);
      updateResetToggles();
      if (midiOutNode.onNodeDirtied)
        midiOutNode.onNodeDirtied();
    };
    addAndMakeVisible(syncModeBox);
    updateResetToggles();

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

    // --- Visualization ---
    addAndMakeVisible(visualizer);

    patternTitle.setText("PATTERN", juce::dontSendNotification);
    patternTitle.setJustificationType(juce::Justification::centred);
    patternTitle.setFont(juce::Font(13.0f, juce::Font::bold));
    patternTitle.setColour(juce::Label::textColourId, juce::Colours::magenta);
    addAndMakeVisible(patternTitle);

    rhythmTitle.setText("RHYTHM", juce::dontSendNotification);
    rhythmTitle.setJustificationType(juce::Justification::centred);
    rhythmTitle.setFont(juce::Font(13.0f, juce::Font::bold));
    rhythmTitle.setColour(juce::Label::textColourId, juce::Colours::cyan);
    addAndMakeVisible(rhythmTitle);

    // --- Cycle Length Display ---
    cycleLengthLabel.setJustificationType(juce::Justification::centredLeft);
    cycleLengthLabel.setFont(juce::Font(11.0f));
    cycleLengthLabel.setColour(juce::Label::textColourId,
                               juce::Colours::lightgrey);
    addAndMakeVisible(cycleLengthLabel);
    updateCycleLabel();
    startTimerHz(30); // Higher rate for smooth playhead
    setSize(390, 290);
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
    lPSteps.setText("P STEPS", juce::dontSendNotification);
    lPBeats.setText("P BEATS", juce::dontSendNotification);

    int halfP = (midiOutNode.pSteps + 1) / 2;
    pOffset.setRange(-halfP, halfP, 1);
    pOffset.setValue(midiOutNode.pOffset, juce::dontSendNotification);
    lPOffset.setText("P OFFSET", juce::dontSendNotification);

    // Rhythm
    rBeats.setRange(1, std::max(1, midiOutNode.rSteps), 1);
    rBeats.setValue(midiOutNode.rBeats, juce::dontSendNotification);
    lRSteps.setText("R STEPS", juce::dontSendNotification);
    lRBeats.setText("R BEATS", juce::dontSendNotification);

    int halfR = (midiOutNode.rSteps + 1) / 2;
    rOffset.setRange(-halfR, halfR, 1);
    rOffset.setValue(midiOutNode.rOffset, juce::dontSendNotification);
    lROffset.setText("R OFFSET", juce::dontSendNotification);
  }

  void updateResetToggles() {
    bool isDeterministic =
        midiOutNode.syncMode == MidiOutNode::SyncMode::Deterministic;
    patternResetToggle.setEnabled(!isDeterministic);
    rhythmResetToggle.setEnabled(!isDeterministic);
  }

  void updateCycleLabel() {
    cycleLengthLabel.setText("Cycle: " + midiOutNode.getCycleLengthInfo(),
                             juce::dontSendNotification);
  }

  void resized() override {
    auto bounds = getLocalBounds();
    auto titleArea = bounds.removeFromTop(20);
    // Title bar layout
    int thirdW = titleArea.getWidth() / 3;
    patternTitle.setBounds(titleArea.removeFromLeft(thirdW));
    rhythmTitle.setBounds(titleArea.removeFromRight(thirdW));

    // The proportionate grid (78x54 units)
    const int numCols = 78;
    const int numRows = 54;
    const float unitW = (float)bounds.getWidth() / numCols;
    const float unitH = (float)bounds.getHeight() / numRows;

    juce::Grid grid;
    for (int i = 0; i < numCols; ++i)
      grid.templateColumns.add(juce::Grid::Px(unitW));
    for (int i = 0; i < numRows; ++i)
      grid.templateRows.add(juce::Grid::Px(unitH));

    // --- Grid Ranges (Units) ---
    // Total: 78x54
    // Rows: 1-41 (Body), 41-46 (Ctrl1), 46-51 (Ctrl2), 51-54 (Footer)
    int rowBodyS = 1, rowBodyE = 41;
    int rowCtrl1S = 41, rowCtrl1E = 46;
    int rowCtrl2S = 46, rowCtrl2E = 51;
    int rowFooterS = 51, rowFooterE = 55;

    // Columns (Overlapping for larger rotaries):
    // Left Knobs: 1-35 (175px approx)
    // Visualizer: 21-58 (Centered)
    // Right Knobs: 44-78 (175px approx)
    int colLeftS = 1, colLeftE = 22;
    int colVisS = 18, colVisE = 55;
    int colRightS = 51, colRightE = 75;

    // --- Visualizer ---
    grid.items.add(juce::GridItem(visualizer)
                       .withArea(rowBodyS, colVisS, rowBodyE, colVisE));

    // --- Bottom Controls ---
    auto placeB = [&](juce::Component &c, int rowS, int rowE, int colS,
                      int colE) {
      grid.items.add(juce::GridItem(c)
                         .withArea(rowS, colS, rowE, colE)
                         .withMargin(juce::GridItem::Margin(2)));
    };

    placeB(clockDivBox, rowCtrl1S, rowCtrl1E, 1, 26);
    placeB(tripletToggle, rowCtrl1S, rowCtrl1E, 27, 52);
    placeB(outputChannelBox, rowCtrl1S, rowCtrl1E, 53, 78);

    placeB(syncModeBox, rowCtrl2S, rowCtrl2E, 1, 26);
    placeB(patternResetToggle, rowCtrl2S, rowCtrl2E, 27, 52);
    placeB(rhythmResetToggle, rowCtrl2S, rowCtrl2E, 53, 78);

    // --- Footer ---
    grid.items.add(juce::GridItem(cycleLengthLabel)
                       .withArea(rowFooterS, 1, rowFooterE, numCols + 1));

    // --- Knobs ---
    int knobHeightUnits = (rowBodyE - rowBodyS) / 3;

    auto placeK = [&](juce::Component &l, juce::Component &s, int rowIdx,
                      int colS, int colE, bool shiftRight) {
      int rS = rowBodyS + (rowIdx * knobHeightUnits);
      int rE = rS + knobHeightUnits;

      // Label (2 units = 10px approx)
      juce::GridItem li(l);
      li.row.start = rS;
      li.row.end = rS + 2;
      li.column.start = colS;
      li.column.end = colE;
      if (shiftRight)
        li.margin.left = 26.0f; // Shift center by 13px
      else
        li.margin.right = 26.0f;
      li.justifySelf = juce::GridItem::JustifySelf::center;
      grid.items.add(li);

      // Slider
      juce::GridItem si(s);
      si.row.start = rS + 2;
      si.row.end = rE;
      si.column.start = colS;
      si.column.end = colE;
      if (shiftRight)
        si.margin.left = 26.0f;
      else
        si.margin.right = 26.0f;
      si.justifySelf = juce::GridItem::JustifySelf::center;
      si.alignSelf = juce::GridItem::AlignSelf::center;
      grid.items.add(si);
    };

    placeK(lPSteps, pSteps, 0, colLeftS, colLeftE, true);
    placeK(lPBeats, pBeats, 1, colLeftS, colLeftE, false);
    placeK(lPOffset, pOffset, 2, colLeftS, colLeftE, true);

    placeK(lRSteps, rSteps, 0, colRightS, colRightE, false);
    placeK(lRBeats, rBeats, 1, colRightS, colRightE, true);
    placeK(lROffset, rOffset, 2, colRightS, colRightE, false);

    grid.performLayout(bounds);
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
