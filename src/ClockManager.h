#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

class ClockManager {
public:
  ClockManager() = default;
  ~ClockManager() = default;

  // Called every audio block to advance internal counters
  void update(juce::AudioPlayHead *playHead, int samplesPerBlock,
              double sampleRate);

  // For Step 2: hardcoded true if a 1/16th note boundary was just crossed
  bool isTick() const;

  // Access current BPM
  double getBPM() const { return currentBPM; }

private:
  double currentBPM = 120.0;
  bool tickFlag = false;

  // Internal phase accumulator for when host is not playing or no PlayHead is
  // provided
  double internalPhase = 0.0;

  // Store previous PPQ position to detect crossings
  double lastPpqPosition = -1.0;
};
