#include <catch2/catch_test_macros.hpp>

#include "GraphEngine.h"
#include "ReverseNode/ReverseNode.h"
#include "TransposeNode/TransposeNode.h"

static NoteSequence oneNote(int pitch) {
  HeldNote n;
  n.noteNumber = pitch;
  return {{n}};
}

// ──────────────────────────────────────────────────────────────────────────────
// addExplicitConnection guards
// ──────────────────────────────────────────────────────────────────────────────

TEST_CASE("addExplicitConnection rejects self-connection", "[engine]") {
  GraphEngine e;
  auto t = std::make_shared<TransposeNode>();
  e.addNode(t);
  CHECK_FALSE(e.addExplicitConnection(t.get(), 0, t.get(), 0));
}

TEST_CASE("addExplicitConnection rejects cycle", "[engine]") {
  GraphEngine e;
  auto a = std::make_shared<TransposeNode>();
  auto b = std::make_shared<TransposeNode>();
  e.addNode(a);
  e.addNode(b);

  REQUIRE(e.addExplicitConnection(a.get(), 0, b.get(), 0));
  // Adding b→a would create a→b→a cycle.
  CHECK_FALSE(e.addExplicitConnection(b.get(), 0, a.get(), 0));
}

TEST_CASE("addExplicitConnection rejects out-of-range output port",
          "[engine]") {
  GraphEngine e;
  auto a = std::make_shared<TransposeNode>();
  auto b = std::make_shared<TransposeNode>();
  e.addNode(a);
  e.addNode(b);
  CHECK_FALSE(e.addExplicitConnection(a.get(), 5, b.get(), 0));
}

TEST_CASE("addExplicitConnection rejects out-of-range input port", "[engine]") {
  GraphEngine e;
  auto a = std::make_shared<TransposeNode>();
  auto b = std::make_shared<TransposeNode>();
  e.addNode(a);
  e.addNode(b);
  CHECK_FALSE(e.addExplicitConnection(a.get(), 0, b.get(), 5));
}

TEST_CASE("addExplicitConnection replaces prior source on occupied input",
          "[engine]") {
  GraphEngine e;
  auto src1 = std::make_shared<TransposeNode>();
  auto src2 = std::make_shared<TransposeNode>();
  auto dst = std::make_shared<TransposeNode>();
  e.addNode(src1);
  e.addNode(src2);
  e.addNode(dst);

  REQUIRE(e.addExplicitConnection(src1.get(), 0, dst.get(), 0));
  CHECK(e.isInputPortOccupied(dst.get(), 0));

  // Second connection to the same input replaces the first.
  REQUIRE(e.addExplicitConnection(src2.get(), 0, dst.get(), 0));
  CHECK(e.isInputPortOccupied(dst.get(), 0));

  // src1 should no longer have a connection to dst.
  bool src1StillConnected = false;
  for (const auto &[port, conns] : src1->getConnections()) {
    for (const auto &c : conns) {
      if (c.targetNode == dst.get())
        src1StillConnected = true;
    }
  }
  CHECK_FALSE(src1StillConnected);
}

// ──────────────────────────────────────────────────────────────────────────────
// recalculate() propagation
// ──────────────────────────────────────────────────────────────────────────────

TEST_CASE("recalculate propagates through 3-node chain", "[engine]") {
  // Chain: source(Transpose +5) → middle(Transpose +0) → sink(Reverse)
  GraphEngine e;
  auto src = std::make_shared<TransposeNode>();
  auto mid = std::make_shared<TransposeNode>();
  auto snk = std::make_shared<ReverseNode>();

  src->semitones = 5;
  mid->semitones = 0;

  e.addNode(src);
  e.addNode(mid);
  e.addNode(snk);

  REQUIRE(e.addExplicitConnection(src.get(), 0, mid.get(), 0));
  REQUIRE(e.addExplicitConnection(mid.get(), 0, snk.get(), 0));

  // Manually push data into the source node and mark it dirty.
  src->setInputSequence(0, oneNote(60));
  src->isDirty = true;

  e.recalculate();

  // After propagation the sink should have the transposed note (60+5=65).
  auto &out = snk->getOutputSequence(0);
  REQUIRE(out.size() == 1u);
  REQUIRE(!out[0].empty());
  const auto *n = asNote(out[0][0]);
  REQUIRE(n != nullptr);
  CHECK(n->noteNumber == 65);
}

// ──────────────────────────────────────────────────────────────────────────────
// removeNode
// ──────────────────────────────────────────────────────────────────────────────

TEST_CASE("removeNode clears downstream cached inputs", "[engine]") {
  GraphEngine e;
  auto src = std::make_shared<TransposeNode>();
  auto dst = std::make_shared<TransposeNode>();

  e.addNode(src);
  e.addNode(dst);
  REQUIRE(e.addExplicitConnection(src.get(), 0, dst.get(), 0));

  // Propagate something so dst has a cached input.
  src->setInputSequence(0, oneNote(60));
  src->isDirty = true;
  e.recalculate();
  REQUIRE(!dst->getInputSequence(0).empty());

  // Removing src should clear dst's cached input.
  e.removeNode(src.get());
  CHECK(dst->getInputSequence(0).empty());
}
