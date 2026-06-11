#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "ClockManager.h"

class TransportBar : public juce::Component, private juce::Timer {
 public:
  TransportBar(ClockManager &clockManager);
  ~TransportBar() override;

  void paint(juce::Graphics &g) override;
  void resized() override;
  void timerCallback() override;

 private:
  ClockManager &clock;

  juce::TextButton playStopButton;
  juce::TextButton resetButton;
  juce::Slider bpmSlider;
  juce::Label bpmLabel;
  juce::Label positionLabel;
  juce::Label standaloneLabel;
  juce::String lastPositionText;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportBar)
};
