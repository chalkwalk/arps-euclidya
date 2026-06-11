#include <array>
#include <atomic>
#include <catch2/catch_test_macros.hpp>

#include "ClockManager.h"
#include "GraphEngine.h"
#include "MidiInNode/MidiInNode.h"
#include "MidiOutNode/MidiOutNode.h"
#include "NoteExpressionManager.h"
#include "TransposeNode/TransposeNode.h"

// Null macro array — test binary has no real macro parameters.
static std::array<std::atomic<float> *, 32> nullMacros() {
  std::array<std::atomic<float> *, 32> arr{};
  arr.fill(nullptr);
  return arr;
}

// ──────────────────────────────────────────────────────────────────────────────
// MidiIn → Transpose → MidiOut round-trip
// ──────────────────────────────────────────────────────────────────────────────

TEST_CASE("GraphEngine state round-trips MidiIn→Transpose→MidiOut", "[state]") {
  NoteExpressionManager nem;
  ClockManager clk;

  GraphEngine src;
  auto midiIn = std::make_shared<MidiInNode>(nem);
  auto trp = std::make_shared<TransposeNode>();
  auto midiOut = std::make_shared<MidiOutNode>(nem, clk);

  midiIn->gridX = 1;
  midiIn->gridY = 0;
  trp->gridX = 2;
  trp->gridY = 3;
  trp->semitones = 7;
  midiOut->gridX = 4;
  midiOut->gridY = 0;

  src.addNode(midiIn);
  src.addNode(trp);
  src.addNode(midiOut);
  REQUIRE(src.addExplicitConnection(midiIn.get(), 0, trp.get(), 0));
  REQUIRE(src.addExplicitConnection(trp.get(), 0, midiOut.get(), 0));

  // Capture UUIDs before saving.
  juce::Uuid midInId = midiIn->nodeId;
  juce::Uuid trpId = trp->nodeId;
  juce::Uuid midiOutId = midiOut->nodeId;

  // Save.
  juce::XmlElement graphXml("Graph");
  src.saveState(&graphXml);

  // Load into fresh engine.
  GraphEngine dst;
  auto macros = nullMacros();
  dst.loadState(&graphXml, nem, clk, macros);

  const auto &nodes = dst.getNodes();
  REQUIRE(nodes.size() == 3u);

  // Collect nodes by UUID.
  GraphNode *loadedIn = nullptr, *loadedTrp = nullptr, *loadedOut = nullptr;
  for (const auto &n : nodes) {
    if (n->nodeId == midInId)
      loadedIn = n.get();
    else if (n->nodeId == trpId)
      loadedTrp = n.get();
    else if (n->nodeId == midiOutId)
      loadedOut = n.get();
  }

  REQUIRE(loadedIn != nullptr);
  REQUIRE(loadedTrp != nullptr);
  REQUIRE(loadedOut != nullptr);

  // Types survive.
  CHECK(loadedIn->getName() == "Midi In");
  CHECK(loadedTrp->getName() == "Transpose");
  CHECK(loadedOut->getName() == "Midi Out");

  // Grid positions survive.
  CHECK(loadedIn->gridX == 1);
  CHECK(loadedIn->gridY == 0);
  CHECK(loadedTrp->gridX == 2);
  CHECK(loadedTrp->gridY == 3);
  CHECK(loadedOut->gridX == 4);
  CHECK(loadedOut->gridY == 0);

  // Connections survive.
  CHECK(dst.isInputPortOccupied(loadedTrp, 0));
  CHECK(dst.isInputPortOccupied(loadedOut, 0));
}

// ──────────────────────────────────────────────────────────────────────────────
// Factory patch guard: Init.euclidya loads and yields non-empty graph
// ──────────────────────────────────────────────────────────────────────────────

TEST_CASE("Factory patch Init.euclidya loads without errors",
          "[state][factory]") {
  juce::File patchFile(juce::String(ARPS_TEST_DATA_DIR) +
                       "/Factory/Init.euclidya");

  REQUIRE(patchFile.existsAsFile());

  auto xmlRoot = juce::XmlDocument::parse(patchFile);
  REQUIRE(xmlRoot != nullptr);

  auto *graphXml = xmlRoot->getChildByName("Graph");
  REQUIRE(graphXml != nullptr);

  NoteExpressionManager nem;
  ClockManager clk;
  GraphEngine e;
  auto macros = nullMacros();

  e.loadState(graphXml, nem, clk, macros);
  CHECK(!e.getNodes().empty());
}
