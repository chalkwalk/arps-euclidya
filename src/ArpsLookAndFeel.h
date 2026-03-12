#pragma once

#include <JuceHeader.h>

class ArpsLookAndFeel : public juce::LookAndFeel_V4 {
public:
  ArpsLookAndFeel();

  void drawRotarySlider(juce::Graphics &g, int x, int y, int width, int height,
                        float sliderPos, const float rotaryStartAngle,
                        const float rotaryEndAngle,
                        juce::Slider &slider) override;

  // We can also override other layout/drawing methods here as needed for Phase
  // 4

  // Custom colors we can reference globally
  static const juce::Colour getNeonColor();
  static const juce::Colour getBackgroundCharcoal();
  static const juce::Colour getForegroundSlate();
};
