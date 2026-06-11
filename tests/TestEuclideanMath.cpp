#include <algorithm>
#include <catch2/catch_test_macros.hpp>

#include "EuclideanMath.h"

static int countBeats(const std::vector<bool>& pattern) {
  int n = 0;
  for (bool b : pattern) {
    if (b)
      ++n;
  }
  return n;
}

TEST_CASE("EuclideanMath::generatePattern degenerate inputs", "[euclidean]") {
  // 0 steps — empty pattern, no crash.
  CHECK(EuclideanMath::generatePattern(0, 3, 0).empty());

  // 0 beats — all rests.
  auto allRest = EuclideanMath::generatePattern(8, 0, 0);
  CHECK(allRest.size() == 8);
  CHECK(countBeats(allRest) == 0);

  // beats >= steps — all beats.
  auto allBeat = EuclideanMath::generatePattern(4, 4, 0);
  CHECK(allBeat.size() == 4);
  CHECK(countBeats(allBeat) == 4);

  auto saturated = EuclideanMath::generatePattern(4, 8, 0);
  CHECK(saturated.size() == 4);
  CHECK(countBeats(saturated) == 4);
}

TEST_CASE("EuclideanMath::generatePattern beat count invariant",
          "[euclidean]") {
  // popcount must always equal the requested beat count (when 0 < beats <
  // steps).
  for (int steps = 1; steps <= 16; ++steps) {
    for (int beats = 1; beats < steps; ++beats) {
      auto p = EuclideanMath::generatePattern(steps, beats, 0);
      REQUIRE((int)p.size() == steps);
      CHECK(countBeats(p) == beats);
    }
  }
}

TEST_CASE("EuclideanMath::generatePattern E(3,8) canonical shape",
          "[euclidean]") {
  // The canonical Euclidean(3,8) rhythm has the pattern 10010010 (or a
  // rotation). This implementation starts at a specific rotation — assert the
  // beat count and that beats are distributed with at most one gap-length
  // difference.
  auto p = EuclideanMath::generatePattern(8, 3, 0);
  REQUIRE(p.size() == 8);
  REQUIRE(countBeats(p) == 3);

  // Verify maximal evenness: consecutive beat positions differ by at most 1.
  std::vector<int> beatPos;
  for (int i = 0; i < 8; ++i) {
    if (p[(size_t)i])
      beatPos.push_back(i);
  }
  REQUIRE(beatPos.size() == 3u);
  int gap1 = beatPos[1] - beatPos[0];
  int gap2 = beatPos[2] - beatPos[1];
  int gap3 = 8 - beatPos[2] + beatPos[0];
  int minGap = std::min({gap1, gap2, gap3});
  int maxGap = std::max({gap1, gap2, gap3});
  CHECK(maxGap - minGap <= 1);
}

TEST_CASE("EuclideanMath::generatePattern E(5,8) canonical shape",
          "[euclidean]") {
  auto p = EuclideanMath::generatePattern(8, 5, 0);
  REQUIRE(p.size() == 8);
  REQUIRE(countBeats(p) == 5);
}

TEST_CASE("EuclideanMath::generatePattern offset rotation", "[euclidean]") {
  auto base = EuclideanMath::generatePattern(8, 3, 0);
  auto shifted1 = EuclideanMath::generatePattern(8, 3, 1);
  auto shifted8 =
      EuclideanMath::generatePattern(8, 3, 8);  // full rotation = same

  // Offset by steps wraps around to same pattern.
  CHECK(shifted8 == base);

  // Offset by 1 is a rotation: last element of base moves to front.
  REQUIRE(base.size() == shifted1.size());
  for (size_t i = 0; i < base.size(); ++i) {
    CHECK(shifted1[i] == base[(i + base.size() - 1) % base.size()]);
  }
}

TEST_CASE("EuclideanMath::generatePattern negative offset wraps",
          "[euclidean]") {
  auto pos = EuclideanMath::generatePattern(8, 3, 1);
  auto neg = EuclideanMath::generatePattern(8, 3, -7);  // -7 mod 8 == 1
  CHECK(pos == neg);
}
