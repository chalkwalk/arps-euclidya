#include "EuclideanMath.h"
#include <algorithm>

std::vector<bool> EuclideanMath::generatePattern(int steps, int beats,
                                                 int offset) {
  std::vector<bool> pattern;

  if (steps <= 0)
    return pattern;
  if (beats <= 0) {
    pattern.assign((size_t)steps, false);
    return pattern;
  }
  if (beats >= steps) {
    pattern.assign((size_t)steps, true);
    return pattern;
  }

  // Bresenham-style distribution
  // We want to distribute 'beats' uniformly across 'steps' bins.
  // E.g., a line from (0,0) to (steps, beats).
  // A step is true if the line steps up vertically.

  int error = steps / 2;
  for (int i = 0; i < steps; ++i) {
    error -= beats;
    if (error < 0) {
      pattern.push_back(true);
      error += steps;
    } else {
      pattern.push_back(false);
    }
  }

  // Apply offset (positive offset shifts pattern right)
  if (!pattern.empty() && offset != 0) {
    int shift = offset % steps;
    if (shift < 0)
      shift += steps;

    // Shift right means elements from the end move to the beginning
    std::rotate(pattern.rbegin(), pattern.rbegin() + shift, pattern.rend());
  }

  return pattern;
}
