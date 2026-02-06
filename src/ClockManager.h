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

  // A cumulative PPQ counter that always increments, regardless of host state.
  // When host is playing, mirrors host PPQ. When stopped, synthesizes PPQ
  // from BPM. This provides a unified timeline for per-node tick detection.
  double getCumulativePpq() const { return cumulativePpq; }

  bool isHostPlaying() const { return hostPlaying; }

private:
  double currentBPM = 120.0;
  bool tickFlag = false;
  bool hostPlaying = false;

  // Cumulative PPQ: always-incrementing, works in all modes
  double cumulativePpq = 0.0;

  // Internal phase accumulator for free-running mode (samples)
  double internalPhase = 0.0;

  // Store previous PPQ position to detect crossings (for global isTick)
  double lastPpqPosition = -1.0;
};
