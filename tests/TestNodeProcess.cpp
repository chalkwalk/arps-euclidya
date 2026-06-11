#include <catch2/catch_test_macros.hpp>

#include "DataModel.h"
#include "ReverseNode/ReverseNode.h"
#include "SortNode/SortNode.h"
#include "TransposeNode/TransposeNode.h"
#include "ZipNode/ZipNode.h"

// Build a single-step sequence with one note.
static NoteSequence singleNote(int pitch, float vel = 0.8f) {
  HeldNote n;
  n.noteNumber = pitch;
  n.velocity = vel;
  return {{n}};
}

// Build a sequence from a list of pitches (one note per step).
static NoteSequence pitchSeq(std::initializer_list<int> pitches) {
  NoteSequence seq;
  for (int p : pitches) {
    HeldNote n;
    n.noteNumber = p;
    n.velocity = 0.8f;
    seq.push_back({n});
  }
  return seq;
}

// Extract the first note pitch from step i.
static int pitchAt(const NoteSequence &s, size_t i) {
  REQUIRE(i < s.size());
  REQUIRE(!s[i].empty());
  const auto *n = asNote(s[i][0]);
  REQUIRE(n != nullptr);
  return n->noteNumber;
}

// ──────────────────────────────────────────────────────────────────────────────
// TransposeNode
// ──────────────────────────────────────────────────────────────────────────────

TEST_CASE("TransposeNode shifts notes by semitones", "[node][transpose]") {
  TransposeNode t;
  t.semitones = 5;
  t.setInputSequence(0, singleNote(60));
  t.process();

  auto &out = t.getOutputSequence(0);
  REQUIRE(out.size() == 1u);
  CHECK(pitchAt(out, 0) == 65);
}

TEST_CASE("TransposeNode zero semitones passes through", "[node][transpose]") {
  TransposeNode t;
  t.semitones = 0;
  auto seq = pitchSeq({60, 62, 64});
  t.setInputSequence(0, seq);
  t.process();

  auto &out = t.getOutputSequence(0);
  REQUIRE(out.size() == 3u);
  CHECK(pitchAt(out, 0) == 60);
  CHECK(pitchAt(out, 1) == 62);
  CHECK(pitchAt(out, 2) == 64);
}

TEST_CASE("TransposeNode clips out-of-range notes", "[node][transpose]") {
  TransposeNode t;
  t.semitones = 10;
  t.setInputSequence(0, singleNote(120));  // 120+10 = 130 > 127, clipped
  t.process();

  auto &out = t.getOutputSequence(0);
  REQUIRE(out.size() == 1u);
  // Step should be empty (note was dropped).
  CHECK(out[0].empty());
}

TEST_CASE("TransposeNode empty input yields empty output",
          "[node][transpose]") {
  TransposeNode t;
  t.semitones = 7;
  t.setInputSequence(0, {});
  t.process();
  CHECK(t.getOutputSequence(0).empty());
}

// ──────────────────────────────────────────────────────────────────────────────
// ReverseNode
// ──────────────────────────────────────────────────────────────────────────────

TEST_CASE("ReverseNode reverses step order", "[node][reverse]") {
  ReverseNode r;
  r.setInputSequence(0, pitchSeq({60, 62, 64}));
  r.process();

  auto &out = r.getOutputSequence(0);
  REQUIRE(out.size() == 3u);
  CHECK(pitchAt(out, 0) == 64);
  CHECK(pitchAt(out, 1) == 62);
  CHECK(pitchAt(out, 2) == 60);
}

TEST_CASE("ReverseNode single step is unchanged", "[node][reverse]") {
  ReverseNode r;
  r.setInputSequence(0, singleNote(60));
  r.process();

  auto &out = r.getOutputSequence(0);
  REQUIRE(out.size() == 1u);
  CHECK(pitchAt(out, 0) == 60);
}

// ──────────────────────────────────────────────────────────────────────────────
// SortNode
// ──────────────────────────────────────────────────────────────────────────────

TEST_CASE("SortNode sorts steps by ascending pitch", "[node][sort]") {
  SortNode s;
  s.setInputSequence(0, pitchSeq({80, 40, 60}));
  s.process();

  auto &out = s.getOutputSequence(0);
  REQUIRE(out.size() == 3u);
  CHECK(pitchAt(out, 0) == 40);
  CHECK(pitchAt(out, 1) == 60);
  CHECK(pitchAt(out, 2) == 80);
}

TEST_CASE("SortNode preserves already-sorted input", "[node][sort]") {
  SortNode s;
  s.setInputSequence(0, pitchSeq({30, 50, 70}));
  s.process();

  auto &out = s.getOutputSequence(0);
  REQUIRE(out.size() == 3u);
  CHECK(pitchAt(out, 0) == 30);
  CHECK(pitchAt(out, 1) == 50);
  CHECK(pitchAt(out, 2) == 70);
}

// ──────────────────────────────────────────────────────────────────────────────
// ZipNode — pairwise merge + MpeCondition::tryHull
// ──────────────────────────────────────────────────────────────────────────────

TEST_CASE("ZipNode merges two same-length sequences", "[node][zip]") {
  ZipNode z;
  z.setInputSequence(0, pitchSeq({60, 62}));
  z.setInputSequence(1, pitchSeq({64, 65}));
  z.process();

  auto &out = z.getOutputSequence(0);
  REQUIRE(out.size() == 2u);
  // Each step should have both notes.
  CHECK(out[0].size() == 2u);
  CHECK(out[1].size() == 2u);
}

TEST_CASE("ZipNode extends to longer input length", "[node][zip]") {
  ZipNode z;
  z.setInputSequence(0, pitchSeq({60}));
  z.setInputSequence(1, pitchSeq({64, 65, 67}));
  z.process();

  CHECK(z.getOutputSequence(0).size() == 3u);
}

TEST_CASE("ZipNode merges notes with same pitch using tryHull", "[node][zip]") {
  // Two notes with same pitch but overlapping conditions: should merge to hull.
  HeldNote na, nb;
  na.noteNumber = 60;
  nb.noteNumber = 60;
  na.mpeCondition.intersectX(0.0f, 0.5f);
  nb.mpeCondition.intersectX(0.4f, 1.0f);
  // x ranges [0,0.5) and [0.4,1.0] overlap → tryHull should succeed.

  ZipNode z;
  z.setInputSequence(0, {{na}});
  z.setInputSequence(1, {{nb}});
  z.process();

  auto &out = z.getOutputSequence(0);
  REQUIRE(out.size() == 1u);
  // Should be merged into a single note.
  CHECK(out[0].size() == 1u);
  const auto *merged = asNote(out[0][0]);
  REQUIRE(merged != nullptr);
  CHECK(merged->mpeCondition.xMin == 0.0f);
  CHECK(merged->mpeCondition.xMax == 1.0f);
}

TEST_CASE("ZipNode keeps both notes when conditions are disjoint",
          "[node][zip]") {
  HeldNote na, nb;
  na.noteNumber = 60;
  nb.noteNumber = 60;
  na.mpeCondition.intersectX(0.0f, 0.4f);  // [0, 0.4) exclusive upper
  nb.mpeCondition.intersectX(0.6f,
                             1.0f);  // [0.6, 1.0] fully disjoint from first
  // 0.4 < 0.6 so disjoint, tryHull should fail.

  ZipNode z;
  z.setInputSequence(0, {{na}});
  z.setInputSequence(1, {{nb}});
  z.process();

  auto &out = z.getOutputSequence(0);
  REQUIRE(out.size() == 1u);
  // Both notes kept.
  CHECK(out[0].size() == 2u);
}
