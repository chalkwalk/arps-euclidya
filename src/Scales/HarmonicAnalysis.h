#pragma once

#include "../Tuning/TuningTable.h"

namespace HarmonicAnalysis {

// Returns a consonance score in [0, 1] where 1 = closest to a low harmonic.
// tuning: active tuning (may be nullptr for 12-TET).
// stepCount: steps per octave (e.g. 12 or 19).
// rootStep: reference step (usually 0).
// step: the step to score.
[[nodiscard]] float scoreStep(const TuningTable *tuning, int stepCount,
                              int rootStep, int step);

}  // namespace HarmonicAnalysis
