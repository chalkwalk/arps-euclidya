#pragma once

#include <juce_graphics/juce_graphics.h>

/// Returns the pre-assigned display colour for macro slot macroIndex (0-31).
/// 32 vibrant, evenly-spaced hues at fixed HSV saturation/brightness so each
/// macro is always visually distinct.
inline juce::Colour getMacroColour(int macroIndex) {
  float hue = static_cast<float>(macroIndex % 32) / 32.0f;
  return juce::Colour::fromHSV(hue, 0.85f, 0.90f, 1.0f);
}
