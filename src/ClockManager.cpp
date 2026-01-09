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
    if (info->getBpm().hasValue()) {
      currentBPM = *info->getBpm();
    }

    if (info->getPpqPosition().hasValue()) {
      double currentPpq = *info->getPpqPosition();

      if (lastPpqPosition >= 0.0) {
        // Determine if we crossed a 1/16th note boundary (0.25 PPQ)
        double division = 0.25;

        double previousTick = std::floor(lastPpqPosition / division);
        double currentTick = std::floor(currentPpq / division);

        if (currentTick > previousTick) {
          tickFlag = true;
        }
      }
      lastPpqPosition = currentPpq;
    }
  } else {
    // Fallback: Free-sync internal clock when host is stopped or unavailable
    lastPpqPosition = -1.0; // Reset host sync

    if (sampleRate > 0) {
      double samplesPerBeat = (sampleRate * 60.0) / currentBPM;
      double samplesPerSixteenth = samplesPerBeat * 0.25;

      internalPhase += samplesPerBlock;

      if (internalPhase >= samplesPerSixteenth) {
        internalPhase -= samplesPerSixteenth;
        tickFlag = true;
      }
    }
  }
}

bool ClockManager::isTick() const { return tickFlag; }
