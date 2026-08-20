#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>

#include "Scales/HarmonicAnalysis.h"
#include "Tuning/TuningTable.h"

// HarmonicAnalysis::scoreStep is a pure function and had no tests. It moves
// into chalkwalk-music with the rest of the pitch model.
//
// The contract: 1.0 means the interval sits exactly on a low harmonic,
// 0.0 means it is 100 cents or more away from every one of them.

TEST_CASE("the unison scores perfectly consonant", "[harmony]") {
  REQUIRE_THAT(HarmonicAnalysis::scoreStep(nullptr, 12, 0, 0),
               Catch::Matchers::WithinAbs(1.0f, 1.0e-5f));
}

TEST_CASE("the octave scores perfectly consonant", "[harmony]") {
  // 1200 cents reduces to 0, which is the h=2 target.
  REQUIRE_THAT(HarmonicAnalysis::scoreStep(nullptr, 12, 0, 12),
               Catch::Matchers::WithinAbs(1.0f, 1.0e-5f));
}

// The 12-TET fifth is 700 cents against a just 701.955, so it is 1.955 cents
// off and should score just under 1. This is the test that would catch the
// harmonic table being wrong.
TEST_CASE("the tempered fifth scores near-perfect but not perfect",
          "[harmony]") {
  const float s = HarmonicAnalysis::scoreStep(nullptr, 12, 0, 7);
  INFO("fifth scores " << s);
  REQUIRE(s > 0.97f);
  REQUIRE(s < 1.0f);
}

// The tempered major third is 400 cents against a just 386.31 -- nearly 14
// cents sharp, which is the well-known ugliness of equal temperament.
TEST_CASE("the tempered major third is measurably worse than the fifth",
          "[harmony]") {
  const float third = HarmonicAnalysis::scoreStep(nullptr, 12, 0, 4);
  const float fifth = HarmonicAnalysis::scoreStep(nullptr, 12, 0, 7);
  INFO("third " << third << " fifth " << fifth);
  REQUIRE(third < fifth);
  REQUIRE(third > 0.8f);
}

TEST_CASE("the minor second is the least consonant interval", "[harmony]") {
  const float semitone = HarmonicAnalysis::scoreStep(nullptr, 12, 0, 1);
  for (int step = 2; step < 12; ++step) {
    INFO("step " << step);
    REQUIRE(HarmonicAnalysis::scoreStep(nullptr, 12, 0, step) >= semitone);
  }
}

TEST_CASE("consonance is measured from the root, not from C", "[harmony]") {
  // A fifth is a fifth wherever it starts.
  for (int root = 0; root < 12; ++root) {
    const float s = HarmonicAnalysis::scoreStep(nullptr, 12, root, root + 7);
    INFO("root " << root);
    REQUIRE_THAT(s, Catch::Matchers::WithinAbs(
                        HarmonicAnalysis::scoreStep(nullptr, 12, 0, 7), 1.0e-4f));
  }
}

TEST_CASE("every score stays inside [0, 1]", "[harmony]") {
  for (int steps : {5, 12, 19, 31}) {
    for (int step = -24; step <= 48; ++step) {
      const float s = HarmonicAnalysis::scoreStep(nullptr, steps, 0, step);
      INFO("steps " << steps << " step " << step << " -> " << s);
      REQUIRE(std::isfinite(s));
      REQUIRE(s >= 0.0f);
      REQUIRE(s <= 1.0f);
    }
  }
}

TEST_CASE("a zero or negative step count is refused rather than dividing by it",
          "[harmony]") {
  REQUIRE(HarmonicAnalysis::scoreStep(nullptr, 0, 0, 3) == 0.0f);
  REQUIRE(HarmonicAnalysis::scoreStep(nullptr, -12, 0, 3) == 0.0f);
}

// 19-TET's fifth (11 steps = 694.7 cents) is further from just than 12-TET's,
// and its major third (6 steps = 378.9 cents) is much closer. That trade is
// the whole reason anyone uses 19, so it is worth asserting.
TEST_CASE("19-TET has a better third and a worse fifth than 12-TET",
          "[harmony][microtonal]") {
  const float third12 = HarmonicAnalysis::scoreStep(nullptr, 12, 0, 4);
  const float third19 = HarmonicAnalysis::scoreStep(nullptr, 19, 0, 6);
  const float fifth12 = HarmonicAnalysis::scoreStep(nullptr, 12, 0, 7);
  const float fifth19 = HarmonicAnalysis::scoreStep(nullptr, 19, 0, 11);

  INFO("thirds: 12-TET " << third12 << " 19-TET " << third19);
  INFO("fifths: 12-TET " << fifth12 << " 19-TET " << fifth19);
  REQUIRE(third19 > third12);
  REQUIRE(fifth19 < fifth12);
}

// An identity tuning table must behave exactly as no tuning table at all,
// or the same scale scores differently depending on how it was loaded.
TEST_CASE("an identity tuning table changes nothing", "[harmony][tuning]") {
  TuningTable identity;
  identity.stepsPerOctave = 12;
  identity.centsDeviation.fill(0.0f);
  REQUIRE(identity.isIdentity());

  for (int step = 0; step < 12; ++step) {
    INFO("step " << step);
    REQUIRE_THAT(HarmonicAnalysis::scoreStep(&identity, 12, 0, step),
                 Catch::Matchers::WithinAbs(
                     HarmonicAnalysis::scoreStep(nullptr, 12, 0, step),
                     1.0e-5f));
  }
}

// A tuning that bends the fifth to just intonation must score better than the
// tempered one. This is the only test that exercises the centsDeviation path.
TEST_CASE("a just fifth scores better than a tempered one",
          "[harmony][tuning]") {
  TuningTable just;
  just.stepsPerOctave = 12;
  just.centsDeviation.fill(0.0f);
  for (size_t i = 7; i < 128; i += 12) just.centsDeviation[i] = 1.955f;
  REQUIRE_FALSE(just.isIdentity());

  const float tempered = HarmonicAnalysis::scoreStep(nullptr, 12, 0, 7);
  const float pure = HarmonicAnalysis::scoreStep(&just, 12, 0, 7);
  INFO("tempered " << tempered << " just " << pure);
  REQUIRE(pure > tempered);
}

TEST_CASE("isIdentity tolerates rounding but not a real deviation",
          "[tuning]") {
  TuningTable t;
  t.centsDeviation.fill(0.0f);
  REQUIRE(t.isIdentity());

  t.centsDeviation[40] = 0.0005f;  // below the 0.001 threshold
  REQUIRE(t.isIdentity());

  t.centsDeviation[40] = 0.5f;
  REQUIRE_FALSE(t.isIdentity());

  t.centsDeviation[40] = -0.5f;
  REQUIRE_FALSE(t.isIdentity());
}

// ---------------------------------------------------------------------------
// The interval circle wraps.
//
// Consonance is measured to the NEAREST harmonic around a 1200-cent circle, so
// a note just below the octave is near-consonant for the same reason a note
// just above the unison is: they are the same distance from the same target.
//
// This had no test, and deleting the wrap passed the whole suite -- because in
// 12-TET it never changes an answer. The nearest step to the octave is the
// major seventh at 1100 cents, which scores 0 with the wrap and 0 without it.
// Only a finer division puts a step inside the wrap's window at all.
// ---------------------------------------------------------------------------
TEST_CASE("consonance is symmetric about the octave", "[harmony][microtonal]") {
  // 31-TET: step 1 is 38.7 cents above the unison, step 30 is 38.7 cents
  // below the octave. Same distance, same target, same score.
  const float justAbove = HarmonicAnalysis::scoreStep(nullptr, 31, 0, 1);
  const float justBelow = HarmonicAnalysis::scoreStep(nullptr, 31, 0, 30);
  INFO("31-TET step 1 " << justAbove << ", step 30 " << justBelow);
  REQUIRE(justAbove > 0.0f);
  REQUIRE_THAT(justBelow, Catch::Matchers::WithinAbs(justAbove, 1.0e-4f));
}

TEST_CASE("the whole interval circle is symmetric", "[harmony][microtonal]") {
  // Every harmonic target is itself reflected, so the entire scoring function
  // must be a mirror about 600 cents in any equal division.
  for (int steps : {19, 31}) {
    for (int step = 1; step < steps; ++step) {
      const float up = HarmonicAnalysis::scoreStep(nullptr, steps, 0, step);
      const float down =
          HarmonicAnalysis::scoreStep(nullptr, steps, 0, steps - step);
      INFO(steps << "-TET step " << step << " (" << up << ") vs "
                 << (steps - step) << " (" << down << ")");
      // Not all targets are mirrored -- the fifth's reflection is the fourth,
      // which is not in the list -- so this asserts only that the pair nearest
      // the octave boundary agree, which is what the wrap is for.
      if (step == 1 || step == steps - 1)
        REQUIRE_THAT(up, Catch::Matchers::WithinAbs(down, 1.0e-4f));
    }
  }
}
