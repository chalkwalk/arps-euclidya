#include <catch2/catch_test_macros.hpp>

#include "AlgorithmicModulatorNode/AlgorithmicModulatorNode.h"
#include "ClockManager.h"
#include "GraphEngine.h"
#include "MidiOutNode/MidiOutNode.h"
#include "NoteExpressionManager.h"
#include "RouteNode/RouteNode.h"
#include "TransposeNode/TransposeNode.h"
#include "ZipNode/ZipNode.h"

// ──────────────────────────────────────────────────────────────────────────────
// Static port-type declarations
// ──────────────────────────────────────────────────────────────────────────────

TEST_CASE("AlgorithmicModulatorNode outputs CC type", "[porttype]") {
  AlgorithmicModulatorNode n;
  CHECK(n.getOutputPortType(0) == GraphNode::PortType::CC);
}

TEST_CASE("TransposeNode inputs and outputs Notes type", "[porttype]") {
  TransposeNode t;
  CHECK(t.getInputPortType(0) == GraphNode::PortType::Notes);
  CHECK(t.getOutputPortType(0) == GraphNode::PortType::Notes);
}

TEST_CASE("ZipNode is Agnostic on all ports", "[porttype]") {
  ZipNode z;
  CHECK(z.getInputPortType(0) == GraphNode::PortType::Agnostic);
  CHECK(z.getInputPortType(1) == GraphNode::PortType::Agnostic);
  CHECK(z.getOutputPortType(0) == GraphNode::PortType::Agnostic);
}

// ──────────────────────────────────────────────────────────────────────────────
// GraphEngine type-compatibility enforcement
// ──────────────────────────────────────────────────────────────────────────────

TEST_CASE("CC output to Notes input is rejected", "[porttype][engine]") {
  GraphEngine e;
  auto mod = std::make_shared<AlgorithmicModulatorNode>();
  auto trp = std::make_shared<TransposeNode>();
  e.addNode(mod);
  e.addNode(trp);
  // CC output → Notes input must be rejected.
  CHECK_FALSE(e.addExplicitConnection(mod.get(), 0, trp.get(), 0));
}

TEST_CASE("CC output to Agnostic input is accepted", "[porttype][engine]") {
  GraphEngine e;
  auto mod = std::make_shared<AlgorithmicModulatorNode>();
  auto zip = std::make_shared<ZipNode>();
  e.addNode(mod);
  e.addNode(zip);
  CHECK(e.addExplicitConnection(mod.get(), 0, zip.get(), 0));
}

TEST_CASE("Notes output to Agnostic input is accepted", "[porttype][engine]") {
  GraphEngine e;
  auto trp = std::make_shared<TransposeNode>();
  auto zip = std::make_shared<ZipNode>();
  e.addNode(trp);
  e.addNode(zip);
  CHECK(e.addExplicitConnection(trp.get(), 0, zip.get(), 0));
}

// ──────────────────────────────────────────────────────────────────────────────
// Agnostic latching via getEffectiveOutputPortType / getEffectiveInputPortType
// ──────────────────────────────────────────────────────────────────────────────

TEST_CASE("RouteNode Agnostic output latches to connected target type",
          "[porttype][engine]") {
  GraphEngine e;
  auto route = std::make_shared<RouteNode>();
  auto trp = std::make_shared<TransposeNode>();
  e.addNode(route);
  e.addNode(trp);

  // Before connection: Agnostic stays Agnostic.
  CHECK(e.getEffectiveOutputPortType(route.get(), 0) ==
        GraphNode::PortType::Agnostic);

  // After connecting RouteNode → TransposeNode (Notes input), it latches to
  // Notes.
  REQUIRE(e.addExplicitConnection(route.get(), 0, trp.get(), 0));
  CHECK(e.getEffectiveOutputPortType(route.get(), 0) ==
        GraphNode::PortType::Notes);
}
