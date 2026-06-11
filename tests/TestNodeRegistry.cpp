#include <catch2/catch_test_macros.hpp>

#include "ClockManager.h"
#include "NodeFactory.h"
#include "NoteExpressionManager.h"

// ──────────────────────────────────────────────────────────────────────────────
// Registry completeness
// ──────────────────────────────────────────────────────────────────────────────

TEST_CASE("createNode returns non-null for every registered type",
          "[registry]") {
  NoteExpressionManager nem;
  ClockManager clk;
  for (const auto &name : NodeFactory::getAvailableNodeTypes()) {
    auto node = NodeFactory::createNode(name, nem, clk);
    REQUIRE(node != nullptr);
    CHECK(node->getName() == name);
  }
}

TEST_CASE("Every registered name appears in exactly one category",
          "[registry]") {
  auto types = NodeFactory::getAvailableNodeTypes();
  auto cats = NodeFactory::getNodeCategories();

  for (const auto &name : types) {
    int count = 0;
    for (const auto &[cat, names] : cats) {
      for (const auto &n : names) {
        if (n == name)
          ++count;
      }
    }
    INFO("Node \"" << name << "\" appears in " << count << " categories");
    CHECK(count == 1);
  }
}

TEST_CASE("getAvailableNodeTypes count matches registry", "[registry]") {
  auto cats = NodeFactory::getNodeCategories();
  std::size_t total = 0;
  for (const auto &[cat, names] : cats)
    total += names.size();
  CHECK(NodeFactory::getAvailableNodeTypes().size() == total);
}
