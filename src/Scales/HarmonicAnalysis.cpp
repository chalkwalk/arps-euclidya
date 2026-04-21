#include "HarmonicAnalysis.h"

#include <array>
#include <cmath>

namespace HarmonicAnalysis {

float scoreStep(const TuningTable *tuning, int stepCount, int rootStep,
                int step) {
  if (stepCount <= 0) {
    return 0.0f;
  }

  // Cents of a step relative to MIDI note 0, using centsDeviation if present.
  auto centOfStep = [&](int s) -> float {
    if (tuning != nullptr && !tuning->isIdentity()) {
      // s * 100 = ET semitone baseline; centsDeviation corrects to actual pitch.
      return (static_cast<float>(s) * 100.0f) +
             tuning->centsDeviation[(size_t)(s % 128)];
    }
    return static_cast<float>(s) * (1200.0f / static_cast<float>(stepCount));
  };

  // Interval from root to this step, reduced to [0, 1200)
  float interval = centOfStep(step) - centOfStep(rootStep);
  interval = std::fmod(interval, 1200.0f);
  if (interval < 0.0f) {
    interval += 1200.0f;
  }

  // Harmonic targets (octave-reduced, in cents): h ∈ {2, 3, 5, 7, 11, 13}
  static const std::array<float, 6> kHarmonicCents = {
      0.0f,    // h=2 → octave → 0 after reduction
      static_cast<float>(1200.0 * std::log2(3.0 / 2.0)),   // ~702 (fifth)
      static_cast<float>(1200.0 * std::log2(5.0 / 4.0)),   // ~386 (maj 3rd)
      static_cast<float>(1200.0 * std::log2(7.0 / 4.0)),   // ~969 (min 7th)
      static_cast<float>(std::fmod(1200.0 * std::log2(11.0), 1200.0)),  // ~551
      static_cast<float>(std::fmod(1200.0 * std::log2(13.0), 1200.0)),  // ~840
  };

  float minDist = 1200.0f;
  for (float hCents : kHarmonicCents) {
    float dist = std::fabs(interval - hCents);
    if (dist > 600.0f) {
      dist = 1200.0f - dist;
    }
    minDist = std::min(minDist, dist);
  }

  // Map 0 cents → 1.0 (most consonant), 100+ cents → 0.0 (least)
  return 1.0f - std::min(minDist / 100.0f, 1.0f);
}

}  // namespace HarmonicAnalysis
