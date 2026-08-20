#include <catch2/catch_test_macros.hpp>

#include <chalkwalk/music/Euclidean.h>

#include <string>
#include <vector>

// The Euclidean migration, completed.
//
// This project used to carry its own generator (`src/EuclideanMath.cpp`): a
// Bresenham line-drawing distribution with the error term seeded at steps/2.
// Two sibling projects shared a different one. All three now call
// chalkwalk-music (libs/music), and the patterns THIS project produces have
// changed as a result.
//
// The change, measured before it was made:
//
//   - The two formulations were ALWAYS the same necklace. Over lengths 2..64
//     there was not one pair that was anything other than a pure rotation. No
//     rhythm was gained or lost -- only its starting point moved.
//   - They differed in 98 of the 120 patterns of length 16 or under.
//   - The old one frequently missed the downbeat: E(3,8), E(5,12), E(5,16),
//     E(7,16) and E(9,32) all started on a rest. The new one never does.
//
// Why the change was worth making, in the order the reasons carried weight:
//
//   1. **This project's own ring visualiser.** With the centred phase, turning
//      BEATS rotated the ring as well as adding hits, and irregularly -- the
//      implied rotation for 32 steps as pulses went 1..8 was
//      16, 8, 26, 4, 28, 2, 6, 2, with no closed form. Now BEATS only adds and
//      redistributes hits around a fixed twelve o'clock and OFFSET is the only
//      control that rotates. Two controls, two jobs.
//   2. A generator whose default misses the downbeat is surprising.
//   3. The centring was never a design decision. `error = steps / 2` is the
//      textbook Bresenham initialiser for round-to-nearest, inherited with the
//      algorithm rather than chosen.
//
// **The bipolar offset control is unchanged.** `pOffset` and `rOffset` remain
// signed and centred on zero with symmetric travel; only the pattern that zero
// produces has moved. Every pattern reachable before is still reachable -- the
// runtime clamp of [-half, +half] spans every rotation -- so this file also
// asserts that, because "you can still get the old sound" is the claim that
// makes the change safe rather than merely defensible.
//
// The generator itself is tested exhaustively in libs/music. What is here is
// only what is specific to THIS project having changed.

namespace {

std::string render(const std::vector<bool> &p) {
  std::string s;
  s.reserve(p.size());
  for (bool b : p) s += b ? 'x' : '.';
  return s;
}

// What this project used to produce. Kept so the migration is a documented
// before/after rather than a remembered one.
std::string formerlyProduced(int steps, int beats) {
  if (steps <= 0) return {};
  if (beats <= 0) return std::string(static_cast<size_t>(steps), '.');
  if (beats >= steps) return std::string(static_cast<size_t>(steps), 'x');
  std::string out;
  int error = steps / 2;
  for (int i = 0; i < steps; ++i) {
    error -= beats;
    if (error < 0) { out += 'x'; error += steps; }
    else           { out += '.'; }
  }
  return out;
}

}  // namespace

TEST_CASE("patterns now start on the downbeat", "[euclidean][migration]") {
  struct Case { int steps; int beats; const char *now; const char *before; };
  const Case cases[] = {
      { 4, 1, "x..."             , "..x."             },
      { 8, 1, "x......."         , "....x..."         },
      { 8, 3, "x..x..x."         , ".x..x.x."         },
      {12, 5, "x..x.x..x.x."     , ".x.x..x.x.x."     },
      {16, 5, "x...x..x..x..x.." , ".x..x...x..x..x." },
      {16, 7, "x..x.x.x..x.x.x." , ".x.x.x..x.x.x.x." },
  };
  for (const auto &c : cases) {
    INFO("E(" << c.beats << "," << c.steps << ")");
    REQUIRE(render(chalkwalk::music::pattern(c.steps, c.beats)) == c.now);
    REQUIRE(formerlyProduced(c.steps, c.beats) == c.before);   // it really did change
    REQUIRE(std::string(c.now) != std::string(c.before));
  }
}

// The claim that makes the change safe: nothing became unreachable. Every
// pattern this project used to produce is still available, within the offset
// range the UI already exposes.
TEST_CASE("every former pattern is still reachable within the offset clamp",
          "[euclidean][migration]") {
  for (int steps = 2; steps <= 32; ++steps) {
    const int half = (steps + 1) / 2;   // MidiOutNode's runtime clamp
    for (int beats = 1; beats < steps; ++beats) {
      const auto target = formerlyProduced(steps, beats);

      bool reachable = false;
      for (int offset = -half; offset <= half && !reachable; ++offset)
        if (render(chalkwalk::music::pattern(steps, beats, offset)) == target)
          reachable = true;

      INFO("E(" << beats << "," << steps << ") former pattern " << target
                << " within offset +/-" << half);
      REQUIRE(reachable);
    }
  }
}

// The onset count never changed, and must not. This is the property no
// migration is allowed to touch: the density dial means what it always meant.
TEST_CASE("the onset count is untouched by the migration",
          "[euclidean][migration]") {
  for (int steps = 1; steps <= 64; ++steps)
    for (int beats = 0; beats <= steps; ++beats) {
      const auto now = chalkwalk::music::pattern(steps, beats);
      int nowCount = 0;
      for (bool b : now) if (b) ++nowCount;

      const auto before = formerlyProduced(steps, std::min(beats, steps));
      int beforeCount = 0;
      for (char ch : before) if (ch == 'x') ++beforeCount;

      INFO("E(" << beats << "," << steps << ")");
      REQUIRE(nowCount == beforeCount);
    }
}

// BEATS should add hits, not rotate the figure. This is the UX property the
// change was made for, and the ring visualiser is what shows it.
TEST_CASE("turning BEATS no longer rotates the pattern",
          "[euclidean][migration]") {
  for (int steps : {8, 16, 32})
    for (int beats = 1; beats < steps; ++beats) {
      INFO("E(" << beats << "," << steps << ")");
      REQUIRE(chalkwalk::music::pattern(steps, beats)[0]);
    }
}
