#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "DataModel.h"
#include "WalkNode/WalkNode.h"

// WalkNode had NO tests, which is how it kept a determinism bug for as long as
// it did.
//
// It seeds from the sorted input "so it's stable", and it used to draw through
// `std::mt19937` + `std::uniform_real_distribution<float>`. The engine is
// specified by the standard; THE DISTRIBUTION IS NOT. libstdc++ and libc++
// return different values from the same engine state, and the seed itself was
// a `size_t`, which is four bytes on some platforms and eight on others. So
// the walk was stable per toolchain and nowhere else -- a patch saved on Linux
// opened as a different phrase on macOS, silently, because each platform was
// perfectly consistent with itself.
//
// A GOLDEN SEQUENCE WOULD NOT HAVE CAUGHT THAT. It would have passed on
// whichever machine generated it and failed everywhere else, and been blamed
// on the platform. So what is pinned here is the PROPERTY that was actually
// broken -- same input, same walk -- plus the behaviour that makes the node
// worth having. The exact sequence is deliberately not asserted.

namespace {

NoteSequence rampOf(int n) {
  NoteSequence s;
  for (int i = 0; i < n; ++i) {
    HeldNote note;
    note.noteNumber = 60 + i;
    note.velocity = 0.8f;
    s.push_back({note});
  }
  return s;
}

std::vector<int> walk(int length, int skewInt, int inputNotes = 8) {
  WalkNode node;
  node.walkLength = length;
  node.walkSkewInt = skewInt;
  node.setInputSequence(0, rampOf(inputNotes));
  node.process();

  std::vector<int> out;
  for (const auto &step : node.getOutputSequence(0))
    for (const auto &ev : step)
      if (const auto *n = std::get_if<HeldNote>(&ev))
        out.push_back(n->noteNumber);
  return out;
}

}  // namespace

TEST_CASE("the same input walks the same way", "[walk][teeth]") {
  // The property the node claims in its own comment and did not have. Two
  // nodes, constructed separately, given identical input.
  const auto a = walk(16, 0);
  const auto b = walk(16, 0);
  REQUIRE(a.size() == 16u);
  REQUIRE(a == b);
}

TEST_CASE("a different input walks differently", "[walk]") {
  // The other half: if the seed did not reach the output, the test above
  // would pass with a hard-coded sequence.
  REQUIRE(walk(16, 0, 8) != walk(16, 0, 7));
}

TEST_CASE("the walk actually moves", "[walk]") {
  // A generator that returned a constant would satisfy both tests above.
  const auto steps = walk(32, 0);
  bool moved = false;
  for (std::size_t i = 1; i < steps.size(); ++i)
    if (steps[i] != steps[i - 1]) moved = true;
  REQUIRE(moved);
}

TEST_CASE("skew biases which way it goes", "[walk]") {
  // Skew is the node's one musical control: negative favours downward steps,
  // positive upward. Summed over a long walk the difference is unambiguous
  // without asserting any particular path.
  const auto down = walk(64, -100);
  const auto up = walk(64, 100);

  auto drift = [](const std::vector<int> &v) {
    return v.empty() ? 0 : v.back() - v.front();
  };
  REQUIRE(drift(down) < drift(up));
}

TEST_CASE("length is honoured", "[walk]") {
  REQUIRE(walk(1, 0).size() == 1u);
  REQUIRE(walk(7, 0).size() == 7u);
  REQUIRE(walk(64, 0).size() == 64u);
}
