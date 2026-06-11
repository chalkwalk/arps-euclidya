#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "ClockManager.h"
#include "TimelineRuler.h"

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
  juce::TextButton loopButton;
  juce::Slider bpmSlider;
  juce::Label bpmLabel;
  juce::Label positionLabel;
  juce::String lastPositionText;
  TimelineRuler ruler;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportBar)
};
