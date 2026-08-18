#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "DataModel.h"
#include "QuantizerNode/QuantizerNode.h"

// QuantizerNode carries the scale logic that is slated to move into
// chalkwalk-music (../../ECOSYSTEM.md). It had no tests at all. These pin the
// behaviour BEFORE the move, so the extraction can be proved rather than
// hoped for.
//
// The mask is set directly rather than by scale name in most cases, so these
// test the quantiser and not the scale library behind it.

namespace {

// Bit i set means step i is in the scale. Written low-to-high so the test
// reads in the same order as a scale degree list, unlike a binary literal.
uint32_t maskOf(std::initializer_list<int> steps) {
  uint32_t m = 0;
  for (int s : steps) m |= (1u << s);
  return m;
}

constexpr int kIonian[] = {0, 2, 4, 5, 7, 9, 11};

NoteSequence seqOf(std::initializer_list<int> pitches) {
  NoteSequence s;
  for (int p : pitches) {
    HeldNote n;
    n.noteNumber = p;
    n.velocity = 0.8f;
    s.push_back({n});
  }
  return s;
}

std::vector<int> pitchesOf(const NoteSequence &s) {
  std::vector<int> out;
  for (const auto &step : s)
    for (const auto &ev : step)
      if (const auto *n = asNote(ev)) out.push_back(n->noteNumber);
  return out;
}

// Steps in the output, with -1 marking a rest, so tests can distinguish
// "the note was dropped and left a gap" from "the step vanished".
std::vector<int> shapeOf(const NoteSequence &s) {
  std::vector<int> out;
  for (const auto &step : s) {
    if (step.empty()) { out.push_back(-1); continue; }
    const auto *n = asNote(step[0]);
    out.push_back(n != nullptr ? n->noteNumber : -1);
  }
  return out;
}

// QuantizerNode is non-copyable (JUCE leak detector), so it is configured in
// place rather than returned by value.
void configure(QuantizerNode &q, int mode, uint32_t mask, int root = 0,
               int steps = 12, int restOnDrop = 1) {
  q.mode = mode;
  q.rootNote = root;
  q.restOnDrop = restOnDrop;
  q.stepsPerOctave.store(steps);
  q.stepMaskBits.store(mask);
}

}  // namespace

// ---------------------------------------------------------------------------
// The default. If the scale library and the node disagree about what "Ionian"
// is, everything downstream is quietly in the wrong key.
// ---------------------------------------------------------------------------
TEST_CASE("a freshly constructed Quantizer is in Ionian", "[quantizer]") {
  const QuantizerNode q;
  REQUIRE(q.stepsPerOctave.load() == 12);
  const uint32_t mask = q.stepMaskBits.load();

  for (int degree = 0; degree < 12; ++degree) {
    const bool expected =
        std::find(std::begin(kIonian), std::end(kIonian), degree) !=
        std::end(kIonian);
    INFO("degree " << degree << " expected in-scale: " << expected);
    CHECK((((mask >> degree) & 1u) != 0u) == expected);
  }
}

// ---------------------------------------------------------------------------
// Filter mode: out-of-scale notes are removed.
// ---------------------------------------------------------------------------
TEST_CASE("Filter mode keeps in-scale notes and drops the rest",
          "[quantizer][filter]") {
  QuantizerNode q;
  configure(q, 0, maskOf({0, 2, 4, 5, 7, 9, 11}));
  // C D D# E F# G  -- D# and F# are out of C Ionian.
  q.setInputSequence(0, seqOf({60, 62, 63, 64, 66, 67}));
  q.process();

  REQUIRE(shapeOf(q.getOutputSequence(0)) ==
          std::vector<int>{60, 62, -1, 64, -1, 67});
}

TEST_CASE("Filter mode without rest-on-drop shortens the sequence",
          "[quantizer][filter]") {
  QuantizerNode q;
  configure(q, 0, maskOf({0, 2, 4, 5, 7, 9, 11}), 0, 12,
                         /*restOnDrop=*/0);
  q.setInputSequence(0, seqOf({60, 63, 62}));
  q.process();

  REQUIRE(shapeOf(q.getOutputSequence(0)) == std::vector<int>{60, 62});
}

// ---------------------------------------------------------------------------
// Snap mode: out-of-scale notes move to the nearest scale step.
// ---------------------------------------------------------------------------
TEST_CASE("Snap mode moves out-of-scale notes to the nearest degree",
          "[quantizer][snap]") {
  QuantizerNode q;
  configure(q, 1, maskOf({0, 2, 4, 5, 7, 9, 11}));
  // C# -> C or D (tie, see below); D# -> E (1 up beats 2 down);
  // F# -> F or G (tie); A# -> B (1 up beats 2 down).
  q.setInputSequence(0, seqOf({63, 70}));
  q.process();

  REQUIRE(pitchesOf(q.getOutputSequence(0)) == std::vector<int>{64, 71});
}

TEST_CASE("Snap mode never moves a note already in the scale",
          "[quantizer][snap]") {
  QuantizerNode q;
  configure(q, 1, maskOf({0, 2, 4, 5, 7, 9, 11}));
  auto in = seqOf({60, 62, 64, 65, 67, 69, 71});
  q.setInputSequence(0, in);
  q.process();

  REQUIRE(pitchesOf(q.getOutputSequence(0)) ==
          std::vector<int>{60, 62, 64, 65, 67, 69, 71});
}

// Equidistant is the interesting case, and "prefer up" is a deliberate choice
// in the implementation rather than an accident of iteration order.
TEST_CASE("Snap mode breaks a tie upwards", "[quantizer][snap]") {
  QuantizerNode q;
  configure(q, 1, maskOf({0, 2, 4, 5, 7, 9, 11}));
  // C# (61) sits exactly between C (60) and D (62).
  q.setInputSequence(0, seqOf({61}));
  q.process();
  REQUIRE(pitchesOf(q.getOutputSequence(0)) == std::vector<int>{62});

  // F# (66) sits exactly between F (65) and G (67).
  QuantizerNode q2;
  configure(q2, 1, maskOf({0, 2, 4, 5, 7, 9, 11}));
  q2.setInputSequence(0, seqOf({66}));
  q2.process();
  REQUIRE(pitchesOf(q2.getOutputSequence(0)) == std::vector<int>{67});
}

TEST_CASE("Snap mode collapses notes that land on the same degree",
          "[quantizer][snap]") {
  QuantizerNode q;
  configure(q, 1, maskOf({0, 4, 7}));  // C major triad only
  NoteSequence in;
  EventStep step;
  for (int p : {60, 61, 62}) {  // 61 and 62 both snap toward 64... or 60
    HeldNote n;
    n.noteNumber = p;
    n.velocity = 0.8f;
    step.push_back(n);
  }
  in.push_back(step);
  q.setInputSequence(0, in);
  q.process();

  const auto out = pitchesOf(q.getOutputSequence(0));
  // Whatever they snap to, no pitch may appear twice in one step: a doubled
  // note is a stuck note downstream.
  for (size_t i = 1; i < out.size(); ++i) REQUIRE(out[i] != out[i - 1]);
}

TEST_CASE("output notes within a step come out sorted by pitch",
          "[quantizer]") {
  QuantizerNode q;
  configure(q, 0, maskOf({0, 2, 4, 5, 7, 9, 11}));
  NoteSequence in;
  EventStep step;
  for (int p : {67, 60, 64}) {
    HeldNote n;
    n.noteNumber = p;
    n.velocity = 0.8f;
    step.push_back(n);
  }
  in.push_back(step);
  q.setInputSequence(0, in);
  q.process();

  REQUIRE(pitchesOf(q.getOutputSequence(0)) == std::vector<int>{60, 64, 67});
}

// ---------------------------------------------------------------------------
// Root note: the mask is relative to the root, not to C.
// ---------------------------------------------------------------------------
TEST_CASE("the root note rotates the scale", "[quantizer]") {
  // D Ionian: D E F# G A B C#  ->  62 64 66 67 69 71 73
  QuantizerNode q;
  configure(q, 0, maskOf({0, 2, 4, 5, 7, 9, 11}), /*root=*/2);
  q.setInputSequence(0, seqOf({62, 64, 66, 67, 69, 71, 73}));
  q.process();
  REQUIRE(pitchesOf(q.getOutputSequence(0)) ==
          std::vector<int>{62, 64, 66, 67, 69, 71, 73});

  // F natural (65) is not in D Ionian.
  QuantizerNode q2;
  configure(q2, 0, maskOf({0, 2, 4, 5, 7, 9, 11}), 2, 12,
                          /*restOnDrop=*/0);
  q2.setInputSequence(0, seqOf({65}));
  q2.process();
  REQUIRE(q2.getOutputSequence(0).empty());
}

// ---------------------------------------------------------------------------
// Microtonal: stepsPerOctave is not always 12. This is the part most likely
// to be broken by a careless port into a shared library.
// ---------------------------------------------------------------------------
TEST_CASE("quantising works in a 19-step octave", "[quantizer][microtonal]") {
  QuantizerNode q;
  configure(q, 0, maskOf({0, 3, 6, 8, 11, 14, 17}), 0, /*steps=*/19,
                         /*restOnDrop=*/0);
  // Steps 0 and 6 are in; 1 and 2 are not. Note numbers here are step indices.
  q.setInputSequence(0, seqOf({0, 1, 2, 6}));
  q.process();
  REQUIRE(pitchesOf(q.getOutputSequence(0)) == std::vector<int>{0, 6});
}

TEST_CASE("a 19-step octave wraps at 19, not at 12",
          "[quantizer][microtonal]") {
  QuantizerNode q;
  configure(q, 0, maskOf({0}), 0, 19, /*restOnDrop=*/0);
  // Only step 0 is in scale, so only multiples of 19 survive.
  q.setInputSequence(0, seqOf({0, 12, 19, 24, 38}));
  q.process();
  REQUIRE(pitchesOf(q.getOutputSequence(0)) == std::vector<int>{0, 19, 38});
}

// ---------------------------------------------------------------------------
// Edges.
// ---------------------------------------------------------------------------
TEST_CASE("an empty input gives an empty output", "[quantizer]") {
  QuantizerNode q;
  configure(q, 0, maskOf({0, 2, 4, 5, 7, 9, 11}));
  q.setInputSequence(0, NoteSequence{});
  q.process();
  REQUIRE(q.getOutputSequence(0).empty());
}

TEST_CASE("an existing rest stays a rest", "[quantizer]") {
  QuantizerNode q;
  configure(q, 0, maskOf({0, 2, 4, 5, 7, 9, 11}));
  NoteSequence in;
  in.emplace_back();            // rest
  in.push_back(seqOf({60})[0]);  // note
  in.emplace_back();            // rest
  q.setInputSequence(0, in);
  q.process();
  REQUIRE(shapeOf(q.getOutputSequence(0)) == std::vector<int>{-1, 60, -1});
}

TEST_CASE("an empty scale mask drops everything in Filter mode",
          "[quantizer]") {
  QuantizerNode q;
  configure(q, 0, 0u, 0, 12, /*restOnDrop=*/0);
  q.setInputSequence(0, seqOf({60, 62, 64}));
  q.process();
  REQUIRE(q.getOutputSequence(0).empty());
}

TEST_CASE("a chromatic mask passes everything through unchanged",
          "[quantizer]") {
  for (int mode : {0, 1}) {
    QuantizerNode q;
  configure(q, mode, maskOf({0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}));
    q.setInputSequence(0, seqOf({60, 61, 62, 63}));
    q.process();
    INFO("mode " << mode);
    REQUIRE(pitchesOf(q.getOutputSequence(0)) ==
            std::vector<int>{60, 61, 62, 63});
  }
}

TEST_CASE("note velocity and channel survive quantisation", "[quantizer]") {
  QuantizerNode q;
  configure(q, 1, maskOf({0, 4, 7}));
  NoteSequence in;
  HeldNote n;
  n.noteNumber = 61;  // out of scale, will be snapped
  n.velocity = 0.42f;
  n.channel = 5;
  in.push_back({n});
  q.setInputSequence(0, in);
  q.process();

  const auto &out = q.getOutputSequence(0);
  REQUIRE(out.size() == 1u);
  REQUIRE(!out[0].empty());
  const auto *got = asNote(out[0][0]);
  REQUIRE(got != nullptr);
  CHECK(got->velocity == 0.42f);
  CHECK(got->channel == 5);
  CHECK(got->noteNumber != 61);
}

// ---------------------------------------------------------------------------
// Negative pitch classes.
//
// `(noteNumber - root) % steps` is NEGATIVE in C++ whenever the note sits
// below the root within its octave, and a negative shift into the mask is
// undefined behaviour. The wrap that fixes this had no test until a deliberate
// sabotage -- deleting the wrap entirely -- passed the whole suite.
// ---------------------------------------------------------------------------
TEST_CASE("notes below the root still quantise correctly",
          "[quantizer][wrap]") {
  // G Ionian (root 7): G A B C D E F#. Note numbers 0..11 are an octave whose
  // pitch classes all fall BELOW the root, so every one of them takes the
  // negative branch.
  QuantizerNode q;
  configure(q, 0, maskOf({0, 2, 4, 5, 7, 9, 11}), /*root=*/7, 12,
            /*restOnDrop=*/0);
  // Relative to G: 0=F(5), 2=G(0), 4=A(2), 5=A#(3, out), 7=C(5), 11=E(9).
  q.setInputSequence(0, seqOf({0, 2, 4, 5, 7, 11}));
  q.process();
  // A# (note 5, five semitones above... i.e. pitch class 10 relative to G) is
  // the only one out of G Ionian.
  REQUIRE(pitchesOf(q.getOutputSequence(0)) ==
          std::vector<int>{0, 2, 4, 7, 11});
}

TEST_CASE("every root gives the same shape for a transposed input",
          "[quantizer][wrap]") {
  // The scale is a rotation, so filtering a chromatic octave must always leave
  // exactly the seven scale degrees, whatever the root and however low the
  // notes. Sweeping low note numbers is what exercises the negative branch.
  for (int root = 0; root < 12; ++root) {
    QuantizerNode q;
    configure(q, 0, maskOf({0, 2, 4, 5, 7, 9, 11}), root, 12,
              /*restOnDrop=*/0);
    NoteSequence in;
    for (int p = 0; p < 12; ++p) {
      HeldNote n;
      n.noteNumber = p;
      n.velocity = 0.8f;
      in.push_back({n});
    }
    q.setInputSequence(0, in);
    q.process();
    INFO("root " << root);
    REQUIRE(pitchesOf(q.getOutputSequence(0)).size() == 7u);
  }
}

TEST_CASE("snapping works below the root too", "[quantizer][wrap][snap]") {
  QuantizerNode q;
  configure(q, 1, maskOf({0, 4, 7}), /*root=*/9);  // A major triad: A C# E
  // Note 0 is C. Relative to A that is pitch class 3; nearest triad degrees
  // are 0 (A) at -3 and 4 (C#) at +1, so it snaps up to C# = note 1.
  q.setInputSequence(0, seqOf({0}));
  q.process();
  REQUIRE(pitchesOf(q.getOutputSequence(0)) == std::vector<int>{1});
}
