#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <vector>

#include "ChordNNode/ChordNNode.h"
#include "ConcatenateNode/ConcatenateNode.h"
#include "DataModel.h"
#include "InterleaveNode/InterleaveNode.h"
#include "OctaveStackNode/OctaveStackNode.h"
#include "OctaveTransposeNode/OctaveTransposeNode.h"
#include "UnzipNode/UnzipNode.h"
#include "VelocityFilterNode/VelocityFilterNode.h"

// Coverage for nodes that had none. Before this file, four of the thirty-three
// node types were tested. These are the pure sequence transforms -- the ones
// with behaviour a test can state exactly -- so they are the cheapest coverage
// available and the most likely to be broken by a refactor.

namespace {

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

NoteSequence chordAt(std::initializer_list<int> pitches) {
  EventStep step;
  for (int p : pitches) {
    HeldNote n;
    n.noteNumber = p;
    n.velocity = 0.8f;
    step.push_back(n);
  }
  return NoteSequence{step};
}

std::vector<int> flatPitches(const NoteSequence &s) {
  std::vector<int> out;
  for (const auto &step : s)
    for (const auto &ev : step)
      if (const auto *n = asNote(ev)) out.push_back(n->noteNumber);
  return out;
}

// Per-step view, with -1 for a rest, so a rest is distinguishable from a
// missing step. Several of these nodes promise to preserve rests.
std::vector<int> stepShape(const NoteSequence &s) {
  std::vector<int> out;
  for (const auto &step : s) {
    if (step.empty()) { out.push_back(-1); continue; }
    const auto *n = asNote(step[0]);
    out.push_back(n != nullptr ? n->noteNumber : -1);
  }
  return out;
}

}  // namespace

// ---------------------------------------------------------------------------
// ConcatenateNode
// ---------------------------------------------------------------------------
TEST_CASE("ConcatenateNode joins port 0 then port 1", "[node][concatenate]") {
  ConcatenateNode c;
  c.setInputSequence(0, seqOf({60, 62}));
  c.setInputSequence(1, seqOf({64, 65}));
  c.process();
  REQUIRE(flatPitches(c.getOutputSequence(0)) ==
          std::vector<int>{60, 62, 64, 65});
}

TEST_CASE("ConcatenateNode with one side empty is the other side",
          "[node][concatenate]") {
  ConcatenateNode c;
  c.setInputSequence(0, seqOf({60, 62}));
  c.setInputSequence(1, NoteSequence{});
  c.process();
  REQUIRE(flatPitches(c.getOutputSequence(0)) == std::vector<int>{60, 62});

  ConcatenateNode c2;
  c2.setInputSequence(0, NoteSequence{});
  c2.setInputSequence(1, seqOf({64}));
  c2.process();
  REQUIRE(flatPitches(c2.getOutputSequence(0)) == std::vector<int>{64});
}

TEST_CASE("ConcatenateNode preserves rests from both sides",
          "[node][concatenate]") {
  ConcatenateNode c;
  NoteSequence a = seqOf({60});
  a.emplace_back();  // trailing rest
  c.setInputSequence(0, a);
  c.setInputSequence(1, seqOf({64}));
  c.process();
  REQUIRE(stepShape(c.getOutputSequence(0)) == std::vector<int>{60, -1, 64});
}

// ---------------------------------------------------------------------------
// InterleaveNode
// ---------------------------------------------------------------------------
TEST_CASE("InterleaveNode alternates the two inputs", "[node][interleave]") {
  InterleaveNode n;
  n.setInputSequence(0, seqOf({60, 62}));
  n.setInputSequence(1, seqOf({70, 72}));
  n.process();
  REQUIRE(stepShape(n.getOutputSequence(0)) ==
          std::vector<int>{60, 70, 62, 72});
}

TEST_CASE("InterleaveNode pads the shorter side with rests",
          "[node][interleave]") {
  InterleaveNode n;
  n.setInputSequence(0, seqOf({60, 62, 64}));
  n.setInputSequence(1, seqOf({70}));
  n.process();
  // The output is always twice the longer input; the gaps become rests.
  REQUIRE(stepShape(n.getOutputSequence(0)) ==
          std::vector<int>{60, 70, 62, -1, 64, -1});
}

TEST_CASE("InterleaveNode with an unconnected side yields rests on that side",
          "[node][interleave]") {
  InterleaveNode n;
  n.setInputSequence(0, seqOf({60, 62}));
  n.process();
  REQUIRE(stepShape(n.getOutputSequence(0)) ==
          std::vector<int>{60, -1, 62, -1});
}

// ---------------------------------------------------------------------------
// UnzipNode -- splits each chord into a lower and an upper half.
// ---------------------------------------------------------------------------
TEST_CASE("UnzipNode splits a chord into low and high halves",
          "[node][unzip]") {
  UnzipNode u;
  u.setInputSequence(0, chordAt({60, 64, 67, 72}));
  u.process();
  REQUIRE(flatPitches(u.getOutputSequence(0)) == std::vector<int>{60, 64});
  REQUIRE(flatPitches(u.getOutputSequence(1)) == std::vector<int>{67, 72});
}

TEST_CASE("UnzipNode sorts before splitting, whatever order it was given",
          "[node][unzip]") {
  UnzipNode u;
  u.setInputSequence(0, chordAt({72, 60, 67, 64}));
  u.process();
  REQUIRE(flatPitches(u.getOutputSequence(0)) == std::vector<int>{60, 64});
  REQUIRE(flatPitches(u.getOutputSequence(1)) == std::vector<int>{67, 72});
}

TEST_CASE("UnzipNode gives an odd chord's extra note to the upper half",
          "[node][unzip]") {
  UnzipNode u;
  u.setInputSequence(0, chordAt({60, 64, 67}));
  u.process();
  REQUIRE(flatPitches(u.getOutputSequence(0)) == std::vector<int>{60});
  REQUIRE(flatPitches(u.getOutputSequence(1)) == std::vector<int>{64, 67});
}

TEST_CASE("UnzipNode sends a lone note low and leaves the high side resting",
          "[node][unzip]") {
  UnzipNode u;
  u.setInputSequence(0, seqOf({60}));
  u.process();
  REQUIRE(flatPitches(u.getOutputSequence(0)) == std::vector<int>{60});
  REQUIRE(stepShape(u.getOutputSequence(1)) == std::vector<int>{-1});
}

TEST_CASE("UnzipNode keeps both outputs the same length", "[node][unzip]") {
  UnzipNode u;
  NoteSequence in = chordAt({60, 64});
  in.emplace_back();                      // rest
  in.push_back(seqOf({67})[0]);           // single note
  u.setInputSequence(0, in);
  u.process();
  REQUIRE(u.getOutputSequence(0).size() == u.getOutputSequence(1).size());
  REQUIRE(u.getOutputSequence(0).size() == 3u);
}

TEST_CASE("UnzipNode on empty input gives two empty outputs", "[node][unzip]") {
  UnzipNode u;
  u.setInputSequence(0, NoteSequence{});
  u.process();
  REQUIRE(u.getOutputSequence(0).empty());
  REQUIRE(u.getOutputSequence(1).empty());
}

// ---------------------------------------------------------------------------
// OctaveTransposeNode
// ---------------------------------------------------------------------------
TEST_CASE("OctaveTransposeNode shifts by whole octaves",
          "[node][octavetranspose]") {
  OctaveTransposeNode o;
  o.octaves = 1;
  o.setInputSequence(0, seqOf({60, 64}));
  o.process();
  REQUIRE(flatPitches(o.getOutputSequence(0)) == std::vector<int>{72, 76});

  OctaveTransposeNode down;
  down.octaves = -2;
  down.setInputSequence(0, seqOf({60}));
  down.process();
  REQUIRE(flatPitches(down.getOutputSequence(0)) == std::vector<int>{36});
}

TEST_CASE("OctaveTransposeNode with zero octaves is a passthrough",
          "[node][octavetranspose]") {
  OctaveTransposeNode o;
  o.octaves = 0;
  o.setInputSequence(0, seqOf({60, 64}));
  o.process();
  REQUIRE(flatPitches(o.getOutputSequence(0)) == std::vector<int>{60, 64});
}

TEST_CASE("OctaveTransposeNode drops notes pushed out of MIDI range, keeping "
          "the step as a rest", "[node][octavetranspose]") {
  OctaveTransposeNode o;
  o.octaves = 4;  // +48
  o.setInputSequence(0, seqOf({60, 100}));  // 108 is fine, 148 is not
  o.process();
  REQUIRE(stepShape(o.getOutputSequence(0)) == std::vector<int>{108, -1});
}

TEST_CASE("OctaveTransposeNode preserves rests", "[node][octavetranspose]") {
  OctaveTransposeNode o;
  o.octaves = 1;
  NoteSequence in = seqOf({60});
  in.emplace_back();
  in.push_back(seqOf({64})[0]);
  o.setInputSequence(0, in);
  o.process();
  REQUIRE(stepShape(o.getOutputSequence(0)) == std::vector<int>{72, -1, 76});
}

// ---------------------------------------------------------------------------
// OctaveStackNode
// ---------------------------------------------------------------------------
TEST_CASE("OctaveStackNode repeats the sequence an octave up each time",
          "[node][octavestack]") {
  OctaveStackNode o;
  o.octaves = 2;
  o.setInputSequence(0, seqOf({60, 62}));
  o.process();
  REQUIRE(flatPitches(o.getOutputSequence(0)) ==
          std::vector<int>{60, 62, 72, 74});
  REQUIRE(o.getOutputSequence(0).size() == 4u);
}

TEST_CASE("OctaveStackNode with one octave is a passthrough",
          "[node][octavestack]") {
  OctaveStackNode o;
  o.octaves = 1;
  o.setInputSequence(0, seqOf({60, 62}));
  o.process();
  REQUIRE(flatPitches(o.getOutputSequence(0)) == std::vector<int>{60, 62});
}

// uniqueOnly is what stops a stacked C4 and an input C5 sounding as a doubled
// note, which reads as a stuck voice rather than a thicker chord.
TEST_CASE("OctaveStackNode drops duplicates when uniqueOnly is set",
          "[node][octavestack]") {
  OctaveStackNode o;
  o.octaves = 2;
  o.uniqueOnly = 1;
  o.setInputSequence(0, seqOf({60, 72}));  // 60+12 == 72, already present
  o.process();
  const auto out = flatPitches(o.getOutputSequence(0));
  auto sorted = out;
  std::sort(sorted.begin(), sorted.end());
  REQUIRE(std::adjacent_find(sorted.begin(), sorted.end()) == sorted.end());
}

TEST_CASE("OctaveStackNode keeps duplicates when uniqueOnly is clear",
          "[node][octavestack]") {
  OctaveStackNode o;
  o.octaves = 2;
  o.uniqueOnly = 0;
  o.setInputSequence(0, seqOf({60, 72}));
  o.process();
  REQUIRE(flatPitches(o.getOutputSequence(0)) ==
          std::vector<int>{60, 72, 72, 84});
}

TEST_CASE("OctaveStackNode never emits a note above 127",
          "[node][octavestack]") {
  OctaveStackNode o;
  o.octaves = 4;
  o.uniqueOnly = 0;
  o.setInputSequence(0, seqOf({120}));
  o.process();
  for (int p : flatPitches(o.getOutputSequence(0))) {
    INFO("pitch " << p);
    REQUIRE(p <= 127);
  }
}

// ---------------------------------------------------------------------------
// ChordNNode -- every N-note combination of the input's unique pitches.
// ---------------------------------------------------------------------------
TEST_CASE("ChordNNode emits every pair from three notes", "[node][chordn]") {
  ChordNNode c;
  c.nValue = 2;
  c.setInputSequence(0, seqOf({60, 64, 67}));
  c.process();
  // C(3,2) = 3 combinations.
  REQUIRE(c.getOutputSequence(0).size() == 3u);
  REQUIRE(flatPitches(c.getOutputSequence(0)) ==
          std::vector<int>{60, 64, 60, 67, 64, 67});
}

TEST_CASE("ChordNNode counts combinations, not permutations",
          "[node][chordn]") {
  ChordNNode c;
  c.nValue = 3;
  c.setInputSequence(0, seqOf({60, 62, 64, 65}));
  c.process();
  REQUIRE(c.getOutputSequence(0).size() == 4u);  // C(4,3)
}

TEST_CASE("ChordNNode pools unique pitches across the whole input",
          "[node][chordn]") {
  ChordNNode c;
  c.nValue = 2;
  c.setInputSequence(0, seqOf({60, 60, 64}));  // 60 appears twice
  c.process();
  REQUIRE(c.getOutputSequence(0).size() == 1u);  // C(2,2)
  REQUIRE(flatPitches(c.getOutputSequence(0)) == std::vector<int>{60, 64});
}

// Asked for a bigger chord than the pool can supply, it gives the biggest one
// it can rather than nothing -- N is clamped to the pool size. Worth stating
// explicitly: the alternative (silence) would be a defensible reading of the
// parameter, and a future refactor could pick it by accident.
TEST_CASE("ChordNNode clamps N to the number of notes available",
          "[node][chordn]") {
  ChordNNode c;
  c.nValue = 5;
  c.setInputSequence(0, seqOf({60, 64}));
  c.process();
  REQUIRE(c.getOutputSequence(0).size() == 1u);
  REQUIRE(flatPitches(c.getOutputSequence(0)) == std::vector<int>{60, 64});
}

TEST_CASE("ChordNNode on empty input gives empty output", "[node][chordn]") {
  ChordNNode c;
  c.nValue = 2;
  c.setInputSequence(0, NoteSequence{});
  c.process();
  REQUIRE(c.getOutputSequence(0).empty());
}

// ---------------------------------------------------------------------------
// VelocityFilterNode -- routes each note to one of two outputs.
// ---------------------------------------------------------------------------
TEST_CASE("VelocityFilterNode splits at the threshold", "[node][velocity]") {
  VelocityFilterNode v;
  v.threshold = 0.5f;

  NoteSequence in;
  for (float vel : {0.9f, 0.2f, 0.5f}) {
    HeldNote n;
    n.noteNumber = 60;
    n.velocity = vel;
    in.push_back({n});
  }
  v.setInputSequence(0, in);
  v.process();

  // >= threshold goes high, so the note exactly at 0.5 goes high.
  REQUIRE(stepShape(v.getOutputSequence(0)) == std::vector<int>{60, -1, 60});
  REQUIRE(stepShape(v.getOutputSequence(1)) == std::vector<int>{-1, 60, -1});
}

// Every note must appear on exactly one side. A note that lands on both is a
// doubled voice; a note that lands on neither is silently lost.
TEST_CASE("VelocityFilterNode loses and duplicates nothing",
          "[node][velocity]") {
  VelocityFilterNode v;
  v.threshold = 0.42f;

  NoteSequence in;
  EventStep step;
  for (int i = 0; i < 10; ++i) {
    HeldNote n;
    n.noteNumber = 60 + i;
    n.velocity = float(i) / 10.0f;
    step.push_back(n);
  }
  in.push_back(step);
  v.setInputSequence(0, in);
  v.process();

  auto all = flatPitches(v.getOutputSequence(0));
  const auto low = flatPitches(v.getOutputSequence(1));
  all.insert(all.end(), low.begin(), low.end());
  std::sort(all.begin(), all.end());

  std::vector<int> expected;
  for (int i = 0; i < 10; ++i) expected.push_back(60 + i);
  REQUIRE(all == expected);
}

TEST_CASE("VelocityFilterNode keeps both outputs aligned with the input",
          "[node][velocity]") {
  VelocityFilterNode v;
  v.threshold = 0.5f;
  NoteSequence in = seqOf({60, 62, 64});
  in.emplace_back();
  v.setInputSequence(0, in);
  v.process();
  // Step indices must line up, or the two halves drift apart in time.
  REQUIRE(v.getOutputSequence(0).size() == in.size());
  REQUIRE(v.getOutputSequence(1).size() == in.size());
}

TEST_CASE("VelocityFilterNode on empty input gives two empty outputs",
          "[node][velocity]") {
  VelocityFilterNode v;
  v.setInputSequence(0, NoteSequence{});
  v.process();
  REQUIRE(v.getOutputSequence(0).empty());
  REQUIRE(v.getOutputSequence(1).empty());
}
