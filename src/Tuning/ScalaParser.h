#pragma once

#include <juce_core/juce_core.h>

#include <vector>

#include "TuningTable.h"

class ScalaParser {
 public:
  struct KbmData {
    int mapSize = 12;
    int firstNote = 0;
    int lastNote = 127;
    int middleNote = 60;
    int refNote = 69;
    double refFreq = 440.0;
    int repeatDegree = 0;  // scale degree at which the period repeats; 0 = use scale size
    std::vector<int> mapping;  // -1 = unmapped ('x')
  };

  // Parse a .scl file and an optional .kbm file into a TuningTable.
  // Pass an invalid/empty kbmFile to use the default mapping.
  static TuningTable parse(const juce::File& sclFile, const juce::File& kbmFile = {});

  // Lower-level helpers, exposed for testing.
  static std::vector<double> parseSclCents(const juce::String& sclText,
                                           juce::String& nameOut);
  static KbmData parseKbm(const juce::String& kbmText);
  static KbmData defaultKbm(int scaleSize);

  static TuningTable computeTable(const std::vector<double>& scaleCents,
                                  const KbmData& kbm, const juce::String& name,
                                  const juce::File& sclFile,
                                  const juce::File& kbmFile);
};
