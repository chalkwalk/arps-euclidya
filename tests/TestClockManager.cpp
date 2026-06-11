#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "ClockManager.h"

static constexpr double kSampleRate = 48000.0;
static constexpr int kBlockSize = 512;
// At BPM=120, one beat = 0.5 s = 24000 samples.
// beatsElapsed per block = 512/24000.
static constexpr double kBeatsPerBlock =
    (double)kBlockSize / ((kSampleRate * 60.0) / 120.0);

static void runBlocks(ClockManager &cm, int n) {
  for (int i = 0; i < n; ++i)
    cm.update(nullptr, kBlockSize, kSampleRate);
}

// ---------------------------------------------------------------------------
// Internal transport: stopped
// ---------------------------------------------------------------------------
TEST_CASE(
    "Internal transport stopped: isHostPlaying false, transportPpq frozen, "
    "cumulativePpq advances",
    "[clock]") {
  ClockManager cm;
  cm.setUseInternalTransport(true);
  cm.setBPM(120.0);
  runBlocks(cm, 10);
  CHECK_FALSE(cm.isHostPlaying());
  CHECK_THAT(cm.getTransportPpq(), Catch::Matchers::WithinAbs(0.0, 1e-9));
  CHECK(cm.getCumulativePpq() > 0.0);
}

// ---------------------------------------------------------------------------
// Internal transport: playing
// ---------------------------------------------------------------------------
TEST_CASE(
    "Internal transport playing: isHostPlaying true, PPQ advances at correct "
    "rate",
    "[clock]") {
  ClockManager cm;
  cm.setUseInternalTransport(true);
  cm.setBPM(120.0);
  cm.setPlaying(true);

  const int n = 100;
  runBlocks(cm, n);
  CHECK(cm.isHostPlaying());
  const double expectedPpq = kBeatsPerBlock * (double)n;
  CHECK_THAT(cm.getTransportPpq(),
             Catch::Matchers::WithinRel(expectedPpq, 0.001));
  CHECK_THAT(cm.getCumulativePpq(),
             Catch::Matchers::WithinRel(expectedPpq, 0.001));
}

// ---------------------------------------------------------------------------
// Pause / resume
// ---------------------------------------------------------------------------
TEST_CASE("Pause/resume: transportPpq holds on pause, resumes from held value",
          "[clock]") {
  ClockManager cm;
  cm.setUseInternalTransport(true);
  cm.setBPM(120.0);
  cm.setPlaying(true);
  runBlocks(cm, 50);
  const double pausedPpq = cm.getTransportPpq();
  cm.setPlaying(false);
  runBlocks(cm, 20);
  CHECK_THAT(cm.getTransportPpq(), Catch::Matchers::WithinAbs(pausedPpq, 1e-9));
  CHECK(cm.getCumulativePpq() > pausedPpq);
  cm.setPlaying(true);
  runBlocks(cm, 50);
  CHECK(cm.getTransportPpq() > pausedPpq);
}

// ---------------------------------------------------------------------------
// resetPhase
// ---------------------------------------------------------------------------
TEST_CASE(
    "resetPhase sets transportPpq to 0, leaves cumulativePpq alone while "
    "stopped",
    "[clock]") {
  ClockManager cm;
  cm.setUseInternalTransport(true);
  cm.setBPM(120.0);
  runBlocks(cm, 100);
  const double freePpq = cm.getCumulativePpq();
  cm.resetPhase();
  CHECK_THAT(cm.getTransportPpq(), Catch::Matchers::WithinAbs(0.0, 1e-9));
  CHECK_THAT(cm.getCumulativePpq(), Catch::Matchers::WithinAbs(freePpq, 1e-9));
}

TEST_CASE("resetPhase while playing: transportPpq resets to 0", "[clock]") {
  ClockManager cm;
  cm.setUseInternalTransport(true);
  cm.setBPM(120.0);
  cm.setPlaying(true);
  runBlocks(cm, 50);
  cm.resetPhase();
  runBlocks(cm, 1);
  CHECK(cm.getTransportPpq() < 0.05);
}

// ---------------------------------------------------------------------------
// seekTransport
// ---------------------------------------------------------------------------
TEST_CASE("seekTransport while stopped: transportPpq updated", "[clock]") {
  ClockManager cm;
  cm.setUseInternalTransport(true);
  cm.seekTransport(8.0);
  CHECK_THAT(cm.getTransportPpq(), Catch::Matchers::WithinAbs(8.0, 1e-9));
  cm.seekTransport(-5.0);
  CHECK_THAT(cm.getTransportPpq(), Catch::Matchers::WithinAbs(0.0, 1e-9));
}

TEST_CASE(
    "seekTransport while playing: next block continues from sought position",
    "[clock]") {
  ClockManager cm;
  cm.setUseInternalTransport(true);
  cm.setBPM(120.0);
  cm.setPlaying(true);
  runBlocks(cm, 10);
  cm.seekTransport(16.0);
  runBlocks(cm, 1);
  CHECK_THAT(cm.getTransportPpq(),
             Catch::Matchers::WithinRel(16.0 + kBeatsPerBlock, 0.001));
}

// ---------------------------------------------------------------------------
// Tick flag
// ---------------------------------------------------------------------------
TEST_CASE("Tick flags fire at 0.5-PPQ crossings while playing", "[clock]") {
  ClockManager cm;
  cm.setUseInternalTransport(true);
  cm.setBPM(120.0);
  cm.setPlaying(true);
  cm.seekTransport(0.49);
  int tickCount = 0;
  for (int i = 0; i < 200; ++i) {
    cm.update(nullptr, kBlockSize, kSampleRate);
    if (cm.isTick())
      ++tickCount;
  }
  CHECK(tickCount > 0);
}

TEST_CASE("No tick flags while transport is stopped", "[clock]") {
  ClockManager cm;
  cm.setUseInternalTransport(true);
  cm.setBPM(120.0);
  int tickCount = 0;
  for (int i = 0; i < 500; ++i) {
    cm.update(nullptr, kBlockSize, kSampleRate);
    if (cm.isTick())
      ++tickCount;
  }
  CHECK(tickCount == 0);
}

// ---------------------------------------------------------------------------
// Regression: JUCE standalone wrapper playhead (time-in-samples only).
// Must fall through to internal transport branch.
// ---------------------------------------------------------------------------
struct StandaloneStylePlayHead : juce::AudioPlayHead {
  juce::Optional<PositionInfo> getPosition() const override {
    PositionInfo info;  // isPlaying defaults to false
    info.setTimeInSamples(int64_t{12345});
    info.setTimeInSeconds(0.26);
    return info;
  }
};

TEST_CASE(
    "JUCE standalone-wrapper playhead falls to internal branch: setPlaying "
    "triggers isHostPlaying",
    "[clock][regression]") {
  ClockManager cm;
  // Deliberately leave useInternalTransport=false to test the hostUsable guard.
  cm.setBPM(120.0);
  cm.setPlaying(true);

  StandaloneStylePlayHead fakeHead;
  cm.update(&fakeHead, kBlockSize, kSampleRate);
  CHECK(cm.isHostPlaying());
  CHECK(cm.getTransportPpq() > 0.0);
}

// ---------------------------------------------------------------------------
// Host-playing branch preserved: full BPM+PPQ+isPlaying → mirrors host.
// ---------------------------------------------------------------------------
struct FullHostPlayHead : juce::AudioPlayHead {
  juce::Optional<PositionInfo> getPosition() const override {
    PositionInfo info;
    info.setIsPlaying(true);
    info.setBpm(140.0);
    info.setPpqPosition(8.25);
    return info;
  }
};

TEST_CASE("Full host playhead: mirrors BPM and PPQ, isHostPlaying true",
          "[clock][regression]") {
  ClockManager cm;
  FullHostPlayHead host;
  cm.update(&host, kBlockSize, kSampleRate);
  CHECK(cm.isHostPlaying());
  CHECK_THAT(cm.getCumulativePpq(), Catch::Matchers::WithinAbs(8.25, 1e-9));
  CHECK_THAT(cm.getBPM(), Catch::Matchers::WithinAbs(140.0, 1e-9));
}

struct FullHostStoppedPlayHead : juce::AudioPlayHead {
  juce::Optional<PositionInfo> getPosition() const override {
    PositionInfo info;
    info.setIsPlaying(false);
    info.setBpm(140.0);
    info.setPpqPosition(8.25);
    return info;
  }
};

TEST_CASE(
    "Full host playhead stopped: isHostPlaying false, cumulativePpq free-runs",
    "[clock][regression]") {
  ClockManager cm;
  FullHostStoppedPlayHead host;
  cm.update(&host, kBlockSize, kSampleRate);
  CHECK_FALSE(cm.isHostPlaying());
  const double ppq0 = cm.getCumulativePpq();
  cm.update(&host, kBlockSize, kSampleRate);
  CHECK(cm.getCumulativePpq() > ppq0);
}

// ---------------------------------------------------------------------------
// Loop region wrap behaviour
// ---------------------------------------------------------------------------
TEST_CASE("Loop: wrap occurs at loopEnd and lands at loopStart + remainder",
          "[clock][loop]") {
  ClockManager cm;
  cm.setUseInternalTransport(true);
  cm.setBPM(120.0);
  cm.setLoopRange(4.0, 8.0);  // bars 2-3 (4 PPQ long)
  cm.setLoopEnabled(true);
  cm.seekTransport(4.0);
  cm.setPlaying(true);

  // Drive blocks until we cross the end (8.0 PPQ)
  double lastPpq = 4.0;
  bool wrapped = false;
  for (int i = 0; i < 500 && !wrapped; ++i) {
    cm.update(nullptr, kBlockSize, kSampleRate);
    const double ppq = cm.getTransportPpq();
    if (ppq < lastPpq) {
      wrapped = true;
      CHECK(ppq >= 4.0);  // must land at loopStart or just after
      CHECK(ppq < 8.0);   // must not overshoot the range
    }
    lastPpq = ppq;
  }
  CHECK(wrapped);
}

TEST_CASE("Loop disabled: no wrap occurs", "[clock][loop]") {
  ClockManager cm;
  cm.setUseInternalTransport(true);
  cm.setBPM(120.0);
  cm.setLoopRange(0.0, 4.0);
  cm.setLoopEnabled(false);
  cm.setPlaying(true);
  runBlocks(cm, 500);
  // Should be well past 4.0 PPQ with no wrap
  CHECK(cm.getTransportPpq() > 4.0);
}

TEST_CASE("Loop: seek past loopEnd while looping does NOT snap back",
          "[clock][loop]") {
  ClockManager cm;
  cm.setUseInternalTransport(true);
  cm.setBPM(120.0);
  cm.setLoopRange(0.0, 4.0);
  cm.setLoopEnabled(true);
  cm.setPlaying(true);
  cm.seekTransport(10.0);  // jump way past end
  cm.update(nullptr, kBlockSize, kSampleRate);
  // First block after seek: no wrap expected because prev < loopEnd was false
  CHECK(cm.getTransportPpq() > 4.0);
}

TEST_CASE("Loop: range shorter than one block does not hang or produce NaN",
          "[clock][loop]") {
  ClockManager cm;
  cm.setUseInternalTransport(true);
  cm.setBPM(240.0);  // fast BPM, large beatsElapsed
  cm.setLoopRange(0.0, 0.01);
  cm.setLoopEnabled(true);
  cm.setPlaying(true);
  // Just check it doesn't crash or produce NaN/inf
  for (int i = 0; i < 20; ++i) {
    cm.update(nullptr, kBlockSize, kSampleRate);
    const double ppq = cm.getTransportPpq();
    CHECK(ppq >= 0.0);
    CHECK(ppq < 1000.0);
  }
}
