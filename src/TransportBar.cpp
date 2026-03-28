#include "TransportBar.h"

#include "ArpsLookAndFeel.h"

TransportBar::TransportBar(ClockManager &clockManager) : clock(clockManager) {
  // Play/Stop toggle
  addAndMakeVisible(playStopButton);
  playStopButton.setClickingTogglesState(true);
  playStopButton.setToggleState(clock.isStandaloneRunning(),
                                juce::dontSendNotification);
  playStopButton.onClick = [this]() {
    bool playing = playStopButton.getToggleState();
    clock.setPlaying(playing);
    playStopButton.setButtonText(playing ? "||" : ">");
  };
  playStopButton.setButtonText(clock.isStandaloneRunning() ? "||" : ">");
  playStopButton.setColour(juce::TextButton::buttonColourId,
                           juce::Colour(0xff333333));
  playStopButton.setColour(juce::TextButton::buttonOnColourId,
                           ArpsLookAndFeel::getNeonColor().withAlpha(0.6f));

  // Reset Button
  addAndMakeVisible(resetButton);
  resetButton.setButtonText("|<");
  resetButton.onClick = [this]() { clock.resetPhase(); };

  // BPM Slider
  addAndMakeVisible(bpmSlider);
  bpmSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
  bpmSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
  bpmSlider.setRange(20.0, 300.0, 1.0);
  bpmSlider.setValue(clock.getBPM(), juce::dontSendNotification);
  bpmSlider.onValueChange = [this]() {
    clock.setBPM(bpmSlider.getValue());
    bpmLabel.setText(juce::String(bpmSlider.getValue(), 1) + " BPM",
                     juce::dontSendNotification);
  };

  addAndMakeVisible(bpmLabel);
  bpmLabel.setText(juce::String(clock.getBPM(), 1) + " BPM",
                   juce::dontSendNotification);
  bpmLabel.setJustificationType(juce::Justification::centredLeft);
  bpmLabel.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
  bpmLabel.setColour(juce::Label::textColourId, juce::Colour(0xffdddddd));

  addAndMakeVisible(standaloneLabel);
  standaloneLabel.setText("STANDALONE TRANSPORT", juce::dontSendNotification);
  standaloneLabel.setJustificationType(juce::Justification::centredRight);
  standaloneLabel.setFont(
      juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
  standaloneLabel.setColour(juce::Label::textColourId,
                            juce::Colour(0xff777777));

  startTimerHz(10);
}

TransportBar::~TransportBar() { stopTimer(); }

void TransportBar::timerCallback() {
  // Sync the UI if BPM was changed externally (e.g. from patch load)
  if (std::abs(bpmSlider.getValue() - clock.getBPM()) > 0.1) {
    bpmSlider.setValue(clock.getBPM(), juce::dontSendNotification);
    bpmLabel.setText(juce::String(clock.getBPM(), 1) + " BPM",
                     juce::dontSendNotification);
  }
}

void TransportBar::paint(juce::Graphics &g) {
  g.fillAll(juce::Colour(0xff1A1A1A));
  g.setColour(juce::Colour(0xff000000));
  g.fillRect(0, getHeight() - 2, getWidth(), 2);  // Bottom shadow border
}

void TransportBar::resized() {
  auto bounds = getLocalBounds().reduced(4);

  auto buttonArea = bounds.removeFromLeft(80);
  playStopButton.setBounds(buttonArea.removeFromLeft(36).reduced(2));
  resetButton.setBounds(buttonArea.removeFromLeft(36).reduced(2));

  bounds.removeFromLeft(10);  // Spacing

  auto bpmArea = bounds.removeFromLeft(120);
  bpmSlider.setBounds(bpmArea.removeFromLeft(36).reduced(2));
  bpmLabel.setBounds(bpmArea);

  standaloneLabel.setBounds(bounds);
}
