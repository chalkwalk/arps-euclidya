#include "TransportBar.h"

#include <cmath>

#include "ArpsLookAndFeel.h"

static juce::String formatBarBeatTick(double ppq) {
  ppq = std::max(0.0, ppq);
  const int bar = (int)(ppq / 4.0) + 1;
  const double beatPos = std::fmod(ppq, 4.0);
  const int beat = (int)beatPos + 1;
  int tick = (int)std::round((beatPos - std::floor(beatPos)) * 960.0);
  if (tick >= 960)
    tick = 0;  // rounding guard at the boundary
  return juce::String(bar) + "." + juce::String(beat) + "." +
         juce::String(tick).paddedLeft('0', 3);
}

TransportBar::TransportBar(ClockManager &clockManager)
    : clock(clockManager), ruler(clockManager) {
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

  // Loop toggle button
  addAndMakeVisible(loopButton);
  loopButton.setButtonText("Loop");
  loopButton.setClickingTogglesState(true);
  loopButton.setToggleState(clock.isLoopEnabled(), juce::dontSendNotification);
  loopButton.onClick = [this]() {
    clock.setLoopEnabled(loopButton.getToggleState());
  };
  loopButton.setColour(juce::TextButton::buttonColourId,
                       juce::Colour(0xff333333));
  loopButton.setColour(juce::TextButton::buttonOnColourId,
                       juce::Colour(0xffaadd00).withAlpha(0.6f));

  // Bar.beat.tick position readout
  addAndMakeVisible(positionLabel);
  positionLabel.setText(formatBarBeatTick(0.0), juce::dontSendNotification);
  positionLabel.setJustificationType(juce::Justification::centredLeft);
  positionLabel.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
  positionLabel.setColour(juce::Label::textColourId, juce::Colour(0xffdddddd));

  addAndMakeVisible(ruler);

  startTimerHz(30);
}

TransportBar::~TransportBar() { stopTimer(); }

void TransportBar::timerCallback() {
  // Sync the UI if BPM was changed externally (e.g. from patch load)
  if (std::abs(bpmSlider.getValue() - clock.getBPM()) > 0.1) {
    bpmSlider.setValue(clock.getBPM(), juce::dontSendNotification);
    bpmLabel.setText(juce::String(clock.getBPM(), 1) + " BPM",
                     juce::dontSendNotification);
  }

  // Sync the Play/Stop button state
  bool isClockRunning = clock.isStandaloneRunning();
  if (playStopButton.getToggleState() != isClockRunning) {
    playStopButton.setToggleState(isClockRunning, juce::dontSendNotification);
    playStopButton.setButtonText(isClockRunning ? "||" : ">");
  }

  // Update bar.beat.tick readout; cache to avoid repaint churn
  juce::String pos = formatBarBeatTick(clock.getTransportPpq());
  if (pos != lastPositionText) {
    lastPositionText = pos;
    positionLabel.setText(pos, juce::dontSendNotification);
  }

  // Sync Loop button
  bool loopOn = clock.isLoopEnabled();
  if (loopButton.getToggleState() != loopOn) {
    loopButton.setToggleState(loopOn, juce::dontSendNotification);
  }

  ruler.tick();
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

  bounds.removeFromLeft(10);  // Spacing

  positionLabel.setBounds(bounds.removeFromLeft(90));

  bounds.removeFromLeft(6);  // Spacing
  loopButton.setBounds(bounds.removeFromLeft(40).reduced(2));

  bounds.removeFromLeft(6);  // Spacing
  ruler.setBounds(bounds.reduced(0, 4));
}
