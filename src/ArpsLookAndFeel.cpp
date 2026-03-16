#include "ArpsLookAndFeel.h"

const juce::Colour ArpsLookAndFeel::getNeonColor() {
  return juce::Colour(0xff0df0e3); // Teal
}
const juce::Colour ArpsLookAndFeel::getBackgroundCharcoal() {
  return juce::Colour(0xff0b1016);
}
const juce::Colour ArpsLookAndFeel::getForegroundSlate() {
  return juce::Colour(0xff121a24);
}

ArpsLookAndFeel::ArpsLookAndFeel() {
  // Setup global color scheme defaults
  setColour(juce::Slider::rotarySliderFillColourId, getNeonColor());
  setColour(juce::Slider::rotarySliderOutlineColourId,
            juce::Colour(0xff333333));

  setColour(juce::TextButton::buttonColourId, getForegroundSlate());
  setColour(juce::TextButton::buttonOnColourId, getNeonColor().withAlpha(0.2f));
  setColour(juce::TextButton::textColourOffId, juce::Colours::white);

  setColour(juce::ComboBox::backgroundColourId, getForegroundSlate());
  setColour(juce::ComboBox::textColourId, juce::Colours::white);
  setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
  setColour(juce::ComboBox::arrowColourId, getNeonColor());

  setColour(juce::PopupMenu::backgroundColourId, getBackgroundCharcoal());
  setColour(juce::PopupMenu::textColourId, juce::Colours::white);
  setColour(juce::PopupMenu::highlightedBackgroundColourId,
            getNeonColor().withAlpha(0.3f));

  setColour(juce::TextEditor::backgroundColourId, getBackgroundCharcoal());
  setColour(juce::TextEditor::textColourId, juce::Colours::white);
  setColour(juce::TextEditor::highlightColourId,
            getNeonColor().withAlpha(0.5f));

  setColour(juce::ListBox::backgroundColourId, getBackgroundCharcoal());
}

void ArpsLookAndFeel::drawRotarySlider(juce::Graphics &g, int x, int y,
                                       int width, int height, float sliderPos,
                                       const float rotaryStartAngle,
                                       const float rotaryEndAngle,
                                       juce::Slider &slider) {
  auto bounds =
      juce::Rectangle<int>(x, y, width, height).toFloat().reduced(2.0f);
  auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f - 2.0f;
  auto centreX = bounds.getCentreX();
  auto centreY = bounds.getCentreY();

  auto angle =
      rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

  // --- Background Glow ---
  g.setGradientFill(
      juce::ColourGradient(getNeonColor().withAlpha(0.05f), centreX, centreY,
                           juce::Colours::transparentBlack, centreX + radius,
                           centreY + radius, true));
  g.fillEllipse(bounds);

  // --- Background Track ---
  juce::Path backgroundArc;
  backgroundArc.addCentredArc(centreX, centreY, radius, radius, 0.0f,
                              rotaryStartAngle, rotaryEndAngle, true);
  g.setColour(slider.findColour(juce::Slider::rotarySliderOutlineColourId));

  float trackWidth = radius * 0.4f; // Scale track width by radius (1/4)
  g.strokePath(backgroundArc,
               juce::PathStrokeType(trackWidth, juce::PathStrokeType::curved,
                                    juce::PathStrokeType::rounded));

  // --- Active Filled Arc (with Neon Glow) ---
  juce::Path filledArc;
  filledArc.addCentredArc(centreX, centreY, radius, radius, 0.0f,
                          rotaryStartAngle, angle, true);

  // Outer glow for the filled arc
  g.setColour(getNeonColor().withAlpha(0.2f));
  g.strokePath(filledArc, juce::PathStrokeType(trackWidth * 1.5f,
                                               juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

  g.setColour(getNeonColor());
  g.strokePath(filledArc,
               juce::PathStrokeType(trackWidth, juce::PathStrokeType::curved,
                                    juce::PathStrokeType::rounded));

  // --- Center Value ---
  auto valueText = slider.getTextFromValue(slider.getValue());
  g.setColour(juce::Colours::white);
  g.setFont(juce::FontOptions(radius * 0.9f).withStyle("Bold"));
  g.drawText(valueText, bounds.translated(0, 1.0f),
             juce::Justification::centred, false);

  // --- Indicator Dot ---
  juce::Point<float> thumbPoint(
      centreX + radius * std::cos(angle - juce::MathConstants<float>::halfPi),
      centreY + radius * std::sin(angle - juce::MathConstants<float>::halfPi));

  g.setColour(juce::Colours::white);
  g.fillEllipse(thumbPoint.x - 3.0f, thumbPoint.y - 3.0f, 6.0f, 6.0f);
}
