#include "ClockManager.h"

#include <cmath>

void ClockManager::update(juce::AudioPlayHead *playHead, int samplesPerBlock,
                          double sampleRate) {
  tickFlag = false;
  lastSampleRate = sampleRate;

  juce::Optional<juce::AudioPlayHead::PositionInfo> info;
  if (!useInternalTransport && playHead != nullptr) {
    info = playHead->getPosition();
  }

  // A position lacking BOTH bpm and ppq is "no usable host transport". The
  // JUCE standalone wrapper installs a playhead that reports exactly that
  // (time-in-samples only, isPlaying=false), which previously trapped us in
  // the host-stopped branch forever. Belt-and-braces alongside
  // useInternalTransport for degenerate hosts.
  const bool hostUsable =
      info.hasValue() &&
      (info->getBpm().hasValue() || info->getPpqPosition().hasValue());

  if (hostUsable && info->getIsPlaying()) {
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
  } else if (hostUsable) {
    hostPlaying = false;
    // Fallback: Continue unified timeline from BPM while host is stopped
    lastPpqPosition = -1.0;
    if (sampleRate > 0) {
      double samplesPerBeat = (sampleRate * 60.0) / currentBPM;
      double beatsElapsed = (double)samplesPerBlock / samplesPerBeat;
      cumulativePpq += beatsElapsed;
    }
  } else {
    // Internal transport: standalone app, unit tests, transport-less hosts.
    hostPlaying = standaloneRunning.load();
    lastPpqPosition = -1.0;
    if (sampleRate <= 0) {
      return;
    }
    const double samplesPerBeat = (sampleRate * 60.0) / currentBPM;
    const double beatsElapsed = (double)samplesPerBlock / samplesPerBeat;
    if (standaloneRunning.load()) {
      const double prev = transportPpq.load();
      double next = prev + beatsElapsed;
      // Loop wrap: if enabled with a valid range and the block crosses the end,
      // wrap back to loopStart + remainder.  Handle a range shorter than one
      // block with fmod to avoid infinite looping.
      if (loopEnabled.load() && loopRangeDefined.load()) {
        const double loopStart = loopStartPpq.load();
        const double loopEnd = loopEndPpq.load();
        if (loopEnd > loopStart && prev < loopEnd && next >= loopEnd) {
          const double loopLen = loopEnd - loopStart;
          next = loopStart + std::fmod(next - loopEnd, loopLen);
        }
      }
      transportPpq.store(next);
      cumulativePpq = next;  // mirror, exactly like the host-playing branch
      // Same 1/8th-note floor-crossing tick test as the host branch.
      const double division = 0.5;
      if (std::floor(next / division) > std::floor(prev / division)) {
        tickFlag = true;
      }
    } else {
      // Free-run while stopped so Gestural mode keeps playing (matches the
      // host-stopped behaviour in a DAW).
      cumulativePpq += beatsElapsed;
    }
  }
}

bool ClockManager::isTick() const { return tickFlag; }
