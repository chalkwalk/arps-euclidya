#include "ClockManager.h"
#include <cmath>

void ClockManager::update(juce::AudioPlayHead *playHead, int samplesPerBlock,
                          double sampleRate) {
  tickFlag = false;

  juce::Optional<juce::AudioPlayHead::PositionInfo> info;

  if (playHead != nullptr) {
    info = playHead->getPosition();
  }

  if (info.hasValue() && info->getIsPlaying()) {
    hostPlaying = true;
    if (info->getBpm().hasValue()) {
      currentBPM = *info->getBpm();
    }

    if (info->getPpqPosition().hasValue()) {
      double currentPpq = *info->getPpqPosition();

      // Update cumulative PPQ to mirror host position
      cumulativePpq = currentPpq;

      if (lastPpqPosition >= 0.0) {
        // Determine if we crossed an 1/8th note boundary (0.5 PPQ)
        double division = 0.5;

        double previousTick = std::floor(lastPpqPosition / division);
        double currentTick = std::floor(currentPpq / division);

        if (currentTick > previousTick) {
          tickFlag = true;
        }
      }
      lastPpqPosition = currentPpq;
    }
  } else {
    hostPlaying = false;
    // Fallback: Free-sync internal clock when host is stopped or unavailable
    lastPpqPosition = -1.0; // Reset host sync

    if (standaloneRunning && sampleRate > 0) {
      double samplesPerBeat = (sampleRate * 60.0) / currentBPM;
      double samplesPerEighth = samplesPerBeat * 0.5;

      internalPhase += samplesPerBlock;

      // Accumulate cumulative PPQ from BPM
      // samplesPerBlock / samplesPerBeat = beats (quarter notes) elapsed
      double beatsElapsed = (double)samplesPerBlock / samplesPerBeat;
      cumulativePpq += beatsElapsed;

      if (internalPhase >= samplesPerEighth) {
        internalPhase -= samplesPerEighth;
        tickFlag = true;
      }
    }
  }
}

bool ClockManager::isTick() const { return tickFlag; }
