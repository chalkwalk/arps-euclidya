#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "DataModel.h"

TEST_CASE("MpeCondition::checkAxis boundaries", "[mpe]") {
  // Lower bound is always inclusive.
  CHECK(MpeCondition::checkAxis(0.3f, 0.3f, 0.7f));
  CHECK_FALSE(MpeCondition::checkAxis(0.29f, 0.3f, 0.7f));

  // Upper bound is exclusive when hi < 1.0.
  CHECK_FALSE(MpeCondition::checkAxis(0.7f, 0.3f, 0.7f));

  // Upper bound is inclusive when hi >= 1.0 (natural ceiling).
  CHECK(MpeCondition::checkAxis(1.0f, 0.0f, 1.0f));
  CHECK(MpeCondition::checkAxis(0.7f, 0.3f, 1.0f));

  // Value at 0 with lo=0 is in range.
  CHECK(MpeCondition::checkAxis(0.0f, 0.0f, 0.5f));

  // Value below lo always out.
  CHECK_FALSE(MpeCondition::checkAxis(0.0f, 0.1f, 0.9f));
}

TEST_CASE("MpeCondition::passes per-axis", "[mpe]") {
  MpeCondition c;
  c.xMin = 0.2f;
  c.xMax = 0.8f;
  c.yMin = 0.0f;
  c.yMax = 0.5f;
  c.zMin = 0.0f;
  c.zMax = 1.0f;

  CHECK(c.passes(0.5f, 0.25f, 0.5f));
  CHECK_FALSE(c.passes(0.1f, 0.25f, 0.5f));  // x below min
  CHECK_FALSE(c.passes(0.5f, 0.5f, 0.5f));   // y at exclusive upper
  CHECK(c.passes(0.5f, 0.0f, 1.0f));         // z at inclusive upper
}

TEST_CASE("MpeCondition::isPassThrough", "[mpe]") {
  MpeCondition def;
  CHECK(def.isPassThrough());

  def.xMin = 0.1f;
  CHECK_FALSE(def.isPassThrough());
}

TEST_CASE("MpeCondition::intersect narrows axes", "[mpe]") {
  MpeCondition c;
  c.intersectX(0.2f, 0.8f);
  CHECK(c.xMin == 0.2f);
  CHECK(c.xMax == 0.8f);

  c.intersectX(0.3f, 0.6f);
  CHECK(c.xMin == 0.3f);
  CHECK(c.xMax == 0.6f);

  // Impossible range (min > max) — always fails passes().
  c.intersectX(0.7f, 0.4f);
  CHECK(c.xMin > c.xMax);
  CHECK_FALSE(c.passes(0.5f, 0.5f, 0.5f));
}

TEST_CASE("MpeCondition::intersectY and intersectZ", "[mpe]") {
  MpeCondition c;
  c.intersectY(0.1f, 0.6f);
  CHECK(c.yMin == 0.1f);
  CHECK(c.yMax == 0.6f);

  c.intersectZ(0.4f, 0.9f);
  CHECK(c.zMin == 0.4f);
  CHECK(c.zMax == 0.9f);
}

TEST_CASE("MpeCondition::tryHull disjoint returns false", "[mpe]") {
  MpeCondition a, b, out;
  a.xMin = 0.0f;
  a.xMax = 0.4f;
  b.xMin = 0.5f;
  b.xMax = 1.0f;
  CHECK_FALSE(MpeCondition::tryHull(a, b, out));
}

TEST_CASE("MpeCondition::tryHull touching returns hull", "[mpe]") {
  MpeCondition a, b, out;
  // x ranges touch at 0.5
  a.xMin = 0.0f;
  a.xMax = 0.5f;
  b.xMin = 0.5f;
  b.xMax = 1.0f;
  // y and z overlap fully
  CHECK(MpeCondition::tryHull(a, b, out));
  CHECK(out.xMin == 0.0f);
  CHECK(out.xMax == 1.0f);
}

TEST_CASE("MpeCondition::tryHull overlapping merges all axes", "[mpe]") {
  MpeCondition a, b, out;
  a.xMin = 0.1f;
  a.xMax = 0.6f;
  b.xMin = 0.4f;
  b.xMax = 0.9f;
  a.yMin = 0.0f;
  a.yMax = 0.5f;
  b.yMin = 0.3f;
  b.yMax = 0.8f;
  CHECK(MpeCondition::tryHull(a, b, out));
  CHECK(out.xMin == 0.1f);
  CHECK(out.xMax == 0.9f);
  CHECK(out.yMin == 0.0f);
  CHECK(out.yMax == 0.8f);
}

TEST_CASE("MpeCondition::tryHull y-disjoint returns false", "[mpe]") {
  MpeCondition a, b, out;
  // x overlaps
  a.xMin = 0.0f;
  a.xMax = 1.0f;
  b.xMin = 0.0f;
  b.xMax = 1.0f;
  // y is disjoint
  a.yMin = 0.0f;
  a.yMax = 0.3f;
  b.yMin = 0.5f;
  b.yMax = 1.0f;
  CHECK_FALSE(MpeCondition::tryHull(a, b, out));
}
