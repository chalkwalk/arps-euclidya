#pragma once

#include <juce_core/juce_core.h>

#include <vector>

struct Scale {
  juce::String name;
  juce::String tuningName;     // "" = 12-TET
  std::vector<bool> stepMask;  // size = steps per octave
};
