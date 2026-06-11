#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>

#include "Tuning/ScalaParser.h"

// ──────────────────────────────────────────────────────────────────────────────
// parseSclCents — valid inputs
// ──────────────────────────────────────────────────────────────────────────────

TEST_CASE("ScalaParser parses 12-TET cents lines", "[scala]") {
  const juce::String scl =
      "! 12tet.scl\n"
      "12 equal temperament\n"
      "12\n"
      "!\n"
      "100.0\n200.0\n300.0\n400.0\n500.0\n600.0\n"
      "700.0\n800.0\n900.0\n1000.0\n1100.0\n1200.0\n";

  juce::String name;
  auto cents = ScalaParser::parseSclCents(scl, name);
  REQUIRE(cents.size() == 12u);
  CHECK_THAT(cents[0], Catch::Matchers::WithinAbs(100.0, 0.001));
  CHECK_THAT(cents[11], Catch::Matchers::WithinAbs(1200.0, 0.001));
}

TEST_CASE("ScalaParser parses ratio lines (3/2 = 701.955 cents)", "[scala]") {
  const juce::String scl =
      "! ratio.scl\n"
      "Test with ratios\n"
      "2\n"
      "!\n"
      "3/2\n"
      "2/1\n";

  juce::String name;
  auto cents = ScalaParser::parseSclCents(scl, name);
  REQUIRE(cents.size() == 2u);
  CHECK_THAT(cents[0], Catch::Matchers::WithinAbs(701.955, 0.01));
  CHECK_THAT(cents[1], Catch::Matchers::WithinAbs(1200.0, 0.01));
}

TEST_CASE("ScalaParser skips comment lines", "[scala]") {
  const juce::String scl =
      "! comments.scl\n"
      "Scale with comments\n"
      "3\n"
      "!\n"
      "! this is a comment\n"
      "200.0\n"
      "! another comment\n"
      "400.0\n"
      "600.0\n";

  juce::String name;
  auto cents = ScalaParser::parseSclCents(scl, name);
  REQUIRE(cents.size() == 3u);
  CHECK_THAT(cents[0], Catch::Matchers::WithinAbs(200.0, 0.001));
}

TEST_CASE("ScalaParser returns name from description line", "[scala]") {
  const juce::String scl =
      "! named.scl\n"
      "My Favourite Scale\n"
      "1\n"
      "100.0\n";

  juce::String name;
  ScalaParser::parseSclCents(scl, name);
  CHECK(name == "My Favourite Scale");
}

// ──────────────────────────────────────────────────────────────────────────────
// parseSclCents — malformed inputs don't crash
// ──────────────────────────────────────────────────────────────────────────────

TEST_CASE("ScalaParser empty input returns empty", "[scala]") {
  juce::String name;
  auto cents = ScalaParser::parseSclCents("", name);
  CHECK(cents.empty());
}

TEST_CASE("ScalaParser garbage input returns empty without crash", "[scala]") {
  juce::String name;
  auto cents =
      ScalaParser::parseSclCents("not a scala file at all\n###\n", name);
  CHECK(cents.size() <= 128u);  // may return partial, but must not crash
}

// ──────────────────────────────────────────────────────────────────────────────
// parseKbm
// ──────────────────────────────────────────────────────────────────────────────

TEST_CASE("ScalaParser parseKbm reads reference frequency", "[scala][kbm]") {
  const juce::String kbm =
      "! default.kbm\n"
      "12\n"
      "0\n"
      "127\n"
      "60\n"
      "69\n"
      "432.0\n"
      "12\n"
      "0\n1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11\n";

  auto kbmData = ScalaParser::parseKbm(kbm);
  CHECK_THAT(kbmData.refFreq, Catch::Matchers::WithinAbs(432.0, 0.001));
  CHECK(kbmData.refNote == 69);
  CHECK(kbmData.middleNote == 60);
}

TEST_CASE("ScalaParser parseKbm handles 'x' unmapped entries", "[scala][kbm]") {
  const juce::String kbm =
      "!\n"
      "7\n"
      "0\n127\n60\n69\n440.0\n7\n"
      "0\nx\n1\nx\n2\nx\n3\n";

  auto kbmData = ScalaParser::parseKbm(kbm);
  REQUIRE(kbmData.mapping.size() == 7u);
  CHECK(kbmData.mapping[1] == -1);  // 'x' mapped to -1
  CHECK(kbmData.mapping[3] == -1);
}

// ──────────────────────────────────────────────────────────────────────────────
// computeTable — 12-TET produces near-zero deviations
// ──────────────────────────────────────────────────────────────────────────────

TEST_CASE("12-TET scale produces near-zero cents deviation", "[scala]") {
  const juce::String scl =
      "! 12tet.scl\n"
      "12 equal temperament\n"
      "12\n"
      "100.0\n200.0\n300.0\n400.0\n500.0\n600.0\n"
      "700.0\n800.0\n900.0\n1000.0\n1100.0\n1200.0\n";

  juce::String name;
  auto cents = ScalaParser::parseSclCents(scl, name);
  auto kbm = ScalaParser::defaultKbm((int)cents.size());
  auto table = ScalaParser::computeTable(cents, kbm, name, {}, {});

  CHECK(table.isIdentity());
}
