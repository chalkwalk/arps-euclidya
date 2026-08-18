#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "EuclideanMath.h"

// The Euclidean disagreement, recorded so it can be settled deliberately.
//
// Three projects generate Euclidean rhythms and they do not agree
// (../ECOSYSTEM.md). Lockstep and Antiphon share one formulation:
//
//     onset at step i  iff  (i * pulses) % length < pulses
//
// This project uses a Bresenham line-drawing formulation with the error term
// seeded at steps/2, which CENTRES the first onset in its bin instead of
// placing it on step 0.
//
// The measured relationship, over lengths 2..64 and every pulse count:
//
//   - The two are ALWAYS the same necklace. There is no pair, anywhere in that
//     range, that is not a pure rotation of the other. So this is a question of
//     phase, never of rhythm.
//   - They nonetheless differ in 98 of the 120 patterns with length <= 16.
//     Agreement is the exception, not the rule.
//   - The other formulation ALWAYS fires on step 0. This one frequently does
//     not: E(3,8), E(5,12), E(5,16), E(7,16) and E(9,32) all start on a rest.
//
// The ecosystem plan adopts the other phase. Three reasons, in order of weight:
//
//   1. This project's own ring visualiser. Turning BEATS with the centred phase
//      ROTATES the ring as well as adding hits, and irregularly -- the implied
//      rotation for 32 steps as pulses go 1..8 is 16, 8, 26, 4, 28, 2, 6, 2,
//      with no closed form. Anchored, BEATS only adds and redistributes hits
//      around a fixed twelve o'clock and OFFSET is the only control that
//      rotates. Two controls, two jobs.
//   2. A generator whose default pattern misses the downbeat is surprising,
//      and Antiphon's kick depends on landing there.
//   3. The centring here was never a design decision. `error = steps / 2` is
//      the textbook Bresenham initialiser for round-to-nearest, inherited with
//      the line-drawing algorithm rather than chosen for musical reasons.
//
// The BIPOLAR OFFSET CONTROL IS UNAFFECTED. `pOffset` and `rOffset` stay
// signed and centred on zero with symmetric travel; only the pattern that zero
// produces changes. Every pattern reachable before is reachable after -- the
// runtime clamp of [-half, +half] already spans every rotation.
//
// No compensating offset will be applied: patterns shift once, deliberately.
// This project has no users yet, which is what makes that free.
//
// These tests pin the current behaviour so that shift is visible. They are
// expected to FAIL when the change lands; that failure is the migration
// checklist, not a regression.

namespace {

std::string render(const std::vector<bool> &p) {
  std::string s;
  s.reserve(p.size());
  for (bool b : p) s += b ? 'x' : '.';
  return s;
}

// What the other two projects produce, for comparison. Not an implementation
// to use -- a reference to measure against.
std::string otherFormulation(int length, int pulses) {
  std::string s;
  for (int i = 0; i < length; ++i)
    s += ((i * pulses) % length) < pulses ? 'x' : '.';
  return s;
}

}  // namespace

TEST_CASE("current Euclidean phase, pinned before it changes",
          "[euclidean][divergence][characterisation]") {
  struct Case { int steps; int beats; const char *expected; };
  const Case cases[] = {
      { 4, 1, "..x."             },
      { 4, 2, ".x.x"             },
      { 4, 3, "x.xx"             },   // agrees with the other formulation
      { 8, 1, "....x..."         },
      { 8, 2, "..x...x."         },
      { 8, 3, ".x..x.x."         },
      { 8, 4, ".x.x.x.x"         },
      { 8, 5, "x.x.xx.x"         },   // agrees with the other formulation
      { 8, 7, "xxx.xxxx"         },
      {12, 3, "..x...x...x."     },
      {12, 4, ".x..x..x..x."     },
      {12, 5, ".x.x..x.x.x."     },
      {16, 4, "..x...x...x...x." },
      {16, 5, ".x..x...x..x..x." },
      {16, 7, ".x.x.x..x.x.x.x." },
  };

  for (const auto &c : cases) {
    const auto got = render(EuclideanMath::generatePattern(c.steps, c.beats, 0));
    INFO("E(" << c.beats << "," << c.steps << ") expected " << c.expected
              << " got " << got);
    REQUIRE(got == c.expected);
  }
}

TEST_CASE("this formulation often misses the downbeat, and that is the "
          "difference", "[euclidean][divergence][characterisation]") {
  // The other two projects place an onset on step 0 for every single pattern.
  // These are the cases where this one does not.
  const int missesDownbeat[][2] = {
      {8, 3}, {12, 5}, {16, 5}, {16, 7}, {32, 9}, {8, 1}, {16, 4},
  };
  for (const auto &c : missesDownbeat) {
    const auto p = EuclideanMath::generatePattern(c[0], c[1], 0);
    INFO("E(" << c[1] << "," << c[0] << ") = " << render(p));
    REQUIRE_FALSE(p[0]);
    // ...whereas the formulation being adopted always does.
    REQUIRE(otherFormulation(c[0], c[1])[0] == 'x');
  }
}

// The reassuring half of the finding: adopting the other phase changes WHERE a
// figure starts, never WHAT it is. No user loses a rhythm; some lose an
// alignment, and an offset restores it.
TEST_CASE("the two formulations are always the same necklace",
          "[euclidean][divergence]") {
  int differing = 0;
  int compared = 0;

  for (int steps = 2; steps <= 48; ++steps)
    for (int beats = 1; beats < steps; ++beats) {
      const auto mine = render(EuclideanMath::generatePattern(steps, beats, 0));
      const auto theirs = otherFormulation(steps, beats);
      ++compared;
      if (mine == theirs) continue;
      ++differing;

      // `mine` must be some rotation of `theirs`.
      bool isRotation = false;
      for (int r = 0; r < steps && !isRotation; ++r) {
        std::string rot = theirs.substr((size_t)(steps - r)) +
                          theirs.substr(0, (size_t)(steps - r));
        if (rot == mine) isRotation = true;
      }
      INFO("E(" << beats << "," << steps << "): " << theirs << " vs " << mine);
      REQUIRE(isRotation);
    }

  INFO(differing << " of " << compared << " patterns differ by rotation");
  REQUIRE(differing > 0);        // they really do disagree
  REQUIRE(compared > 1000);      // and we checked a meaningful range
}

// Whatever phase is chosen, the ONSET COUNT is not in dispute. This is the
// property the shared implementation must preserve for every caller, and the
// one no migration can be allowed to change.
TEST_CASE("onset count is identical in both formulations",
          "[euclidean][divergence]") {
  for (int steps = 1; steps <= 64; ++steps)
    for (int beats = 0; beats <= steps; ++beats) {
      const auto mine = EuclideanMath::generatePattern(steps, beats, 0);
      int mineCount = 0;
      for (bool b : mine) if (b) ++mineCount;

      int theirCount = 0;
      const int clamped = beats > steps ? steps : beats;
      for (int i = 0; i < steps; ++i)
        if (((i * clamped) % steps) < clamped) ++theirCount;

      INFO("E(" << beats << "," << steps << ")");
      REQUIRE(mineCount == theirCount);
    }
}
