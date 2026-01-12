#pragma once

#include <vector>

class EuclideanMath {
public:
  // Generates a boolean sequence using a Bresenham-style line drawing algorithm
  // steps = horizontal pixels, beats = vertical pixels
  static std::vector<bool> generatePattern(int steps, int beats, int offset);
};
