#include <atomic>
#include <catch2/catch_test_macros.hpp>

#include "ClockManager.h"
#include "DataModel.h"
#include "MidiOutNode/MidiOutNode.h"
#include "NoteExpressionManager.h"

// Minimal NoteEventCollector that counts note-ons.
struct CountingCollector : NoteEventCollector {
  int noteOnCount = 0;
  void addNoteOn(int /*channel*/, int /*noteNumber*/, float /*velocity*/,
                 int /*sampleOffset*/, int32_t /*noteID*/) override {
    ++noteOnCount;
  }
  void addNoteOff(int /*channel*/, int /*noteNumber*/, float /*velocity*/,
                  int /*sampleOffset*/, int32_t /*noteID*/) override {}
  void addNoteExpression(int /*channel*/, int /*noteNumber*/,
                         NoteExpressionType /*type*/, float /*value*/,
                         int /*sampleOffset*/, int32_t /*noteID*/) override {}
  void addCC(int /*channel*/, int /*ccNumber*/, int /*value*/,
             int /*sampleOffset*/) override {}
};

// A single note input sequence (1 step, 1 note at pitch 60).
static EventSequence oneNoteSeq() {
  HeldNote n;
  n.noteNumber = 60;
  n.velocity = 0.8f;
  n.channel = 1;
  return {{n}};
}

static constexpr double kSR = 48000.0;
static constexpr int kBlock = 512;
static constexpr int kIterations = 300;  // ~3 s of audio at 48 kHz / 512

// Drive kIterations audio blocks: update clock, process node, collect output.
static int runAndCount(ClockManager &cm, MidiOutNode &node,
                       std::atomic<int32_t> &noteIDCounter) {
  CountingCollector col;
  for (int i = 0; i < kIterations; ++i) {
    cm.update(nullptr, kBlock, kSR);
    node.process();
    node.generateOutput(col, kBlock, noteIDCounter);
  }
  return col.noteOnCount;
}

TEST_CASE("Synchronized mode: note-ons fire while transport is playing",
          "[transport][integration]") {
  NoteExpressionManager nem;
  ClockManager cm;
  cm.setUseInternalTransport(true);
  cm.setBPM(120.0);

  MidiOutNode node(nem, cm);
  node.syncMode = MidiOutNode::SyncMode::Synchronized;
  node.setInputSequence(0, oneNoteSeq());

  // Simulate a held note (NoteExpressionManager side)
  nem.handleNoteOn(1, 60, 0.8f);

  cm.setPlaying(true);

  std::atomic<int32_t> noteIDCounter{0};
  int count = runAndCount(cm, node, noteIDCounter);
  CHECK(count > 0);
}

TEST_CASE("Deterministic mode: note-ons fire while transport is playing",
          "[transport][integration]") {
  NoteExpressionManager nem;
  ClockManager cm;
  cm.setUseInternalTransport(true);
  cm.setBPM(120.0);

  MidiOutNode node(nem, cm);
  node.syncMode = MidiOutNode::SyncMode::Deterministic;
  node.setInputSequence(0, oneNoteSeq());

  nem.handleNoteOn(1, 60, 0.8f);
  cm.setPlaying(true);

  std::atomic<int32_t> noteIDCounter{0};
  int count = runAndCount(cm, node, noteIDCounter);
  CHECK(count > 0);
}

TEST_CASE("Synchronized mode: no note-ons when transport is stopped",
          "[transport][integration]") {
  NoteExpressionManager nem;
  ClockManager cm;
  cm.setUseInternalTransport(true);
  cm.setBPM(120.0);
  // Transport stays stopped (default)

  MidiOutNode node(nem, cm);
  node.syncMode = MidiOutNode::SyncMode::Synchronized;
  node.setInputSequence(0, oneNoteSeq());

  nem.handleNoteOn(1, 60, 0.8f);

  std::atomic<int32_t> noteIDCounter{0};
  int count = runAndCount(cm, node, noteIDCounter);
  CHECK(count == 0);
}
