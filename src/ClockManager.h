#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <algorithm>
#include <atomic>

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
  void setBPM(double bpm) { currentBPM = juce::jlimit(20.0, 300.0, bpm); }

  // A cumulative PPQ counter that always increments, regardless of host state.
  // When host is playing, mirrors host PPQ. When stopped, synthesizes PPQ
  // from BPM. This provides a unified timeline for per-node tick detection.
  // Written and read on the audio thread only — never read from the UI thread.
  double getCumulativePpq() const { return cumulativePpq; }

  bool isHostPlaying() const { return hostPlaying; }
  double getSamplesPerPpq() const {
    if (lastSampleRate > 0) {
      return (lastSampleRate * 60.0) / currentBPM;
    }
    return 1.0;
  }

  // When true, update() ignores any host playhead and runs the internal
  // transport (set once at construction by the processor for standalone).
  void setUseInternalTransport(bool b) { useInternalTransport = b; }

  // The standalone timeline position (what the ruler displays and jogs).
  double getTransportPpq() const { return transportPpq.load(); }
  void seekTransport(double ppq) { transportPpq.store(std::max(0.0, ppq)); }

  // Standalone Transport Controls
  void setPlaying(bool playing) { standaloneRunning.store(playing); }
  bool isStandaloneRunning() const { return standaloneRunning.load(); }
  void resetPhase() {
    transportPpq.store(0.0);
    lastPpqPosition = -1.0;
    // cumulativePpq deliberately NOT zeroed: the free-run timeline keeps
    // continuity while stopped; it re-mirrors transportPpq on the next
    // playing block (MidiOutNode handles the resulting PPQ jump).
  }

  // Loop region (internal transport only)
  void setLoopEnabled(bool b) { loopEnabled.store(b); }
  bool isLoopEnabled() const { return loopEnabled.load(); }
  void setLoopRange(double startPpq, double endPpq) {
    // Enforce end > start; caller should pass bar-snapped values.
    if (endPpq <= startPpq)
      return;
    loopStartPpq.store(startPpq);
    loopEndPpq.store(endPpq);
    loopRangeDefined.store(true);
  }
  double getLoopStartPpq() const { return loopStartPpq.load(); }
  double getLoopEndPpq() const { return loopEndPpq.load(); }
  bool hasLoopRange() const { return loopRangeDefined.load(); }

 private:
  double currentBPM = 120.0;
  bool tickFlag = false;
  bool hostPlaying = false;
  std::atomic<bool> standaloneRunning{false};

  // Cumulative PPQ: always-incrementing, works in all modes.
  // Audio thread only — never read from UI thread.
  double cumulativePpq = 0.0;
  double lastSampleRate = 44100.0;

  // Set once before audio starts; drives update() branch selection.
  bool useInternalTransport = false;

  // Standalone/internal transport position; seekable from UI thread.
  std::atomic<double> transportPpq{0.0};

  // Store previous PPQ position to detect crossings (for global isTick)
  double lastPpqPosition = -1.0;

  // Loop region state (internal transport)
  std::atomic<bool> loopEnabled{false};
  std::atomic<bool> loopRangeDefined{false};
  std::atomic<double> loopStartPpq{0.0};
  std::atomic<double> loopEndPpq{0.0};
};
