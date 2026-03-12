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
  auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat();
  auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f - 2.0f;
  auto centreX = bounds.getCentreX();
  auto centreY = bounds.getCentreY();

  auto angle =
      rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

  // Draw background track
  juce::Path backgroundArc;
  backgroundArc.addCentredArc(centreX, centreY, radius, radius, 0.0f,
                              rotaryStartAngle, rotaryEndAngle, true);
  g.setColour(slider.findColour(juce::Slider::rotarySliderOutlineColourId));
  g.strokePath(backgroundArc,
               juce::PathStrokeType(4.0f, juce::PathStrokeType::curved,
                                    juce::PathStrokeType::rounded));

  // Draw active filled arc
  juce::Path filledArc;
  filledArc.addCentredArc(centreX, centreY, radius, radius, 0.0f,
                          rotaryStartAngle, angle, true);
  g.setColour(slider.findColour(juce::Slider::rotarySliderFillColourId));
  g.strokePath(filledArc,
               juce::PathStrokeType(4.0f, juce::PathStrokeType::curved,
                                    juce::PathStrokeType::rounded));

  // Draw an indicator dot instead of a full pointer line
  juce::Point<float> thumbPoint(
      centreX + radius * std::cos(angle - juce::MathConstants<float>::halfPi),
      centreY + radius * std::sin(angle - juce::MathConstants<float>::halfPi));
  g.fillEllipse(thumbPoint.x - 2.5f, thumbPoint.y - 2.5f, 5.0f, 5.0f);
}
