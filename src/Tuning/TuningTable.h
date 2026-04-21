#pragma once

#include <juce_core/juce_core.h>

#include <array>
#include <cmath>

struct TuningTable {
  std::array<float, 128> centsDeviation{};
  int stepsPerOctave = 12;
  juce::String name;
  juce::File sclFile;
  juce::File kbmFile;

  [[nodiscard]] bool isIdentity() const {
    return std::all_of(centsDeviation.begin(), centsDeviation.end(),
                       [](float c) { return std::fabs(c) <= 0.001f; });
  }
};
