#include <array>
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "GraphNode.h"

// Minimal concrete node for resolution tests.
struct DummyNode : GraphNode {
  std::string getName() const override { return "Dummy"; }
  void process() override {}
  MacroParam paramA{"A", {}};
  DummyNode() { addMacroParam(&paramA); }
};

static void fillAtomics(std::array<std::atomic<float>, 32> &arr, float val) {
  for (auto &a : arr)
    a.store(val);
}

static void wireAtomics(DummyNode &n, std::array<std::atomic<float>, 32> &arr) {
  for (size_t i = 0; i < 32; ++i)
    n.macros[i] = &arr[i];
}

TEST_CASE("resolveMacroInt empty bindings returns clamped localVal",
          "[macro]") {
  DummyNode n;
  CHECK(n.resolveMacroInt(n.paramA, 5, 0, 10) == 5);
  CHECK(n.resolveMacroInt(n.paramA, -3, 0, 10) == 0);   // clamped
  CHECK(n.resolveMacroInt(n.paramA, 15, 0, 10) == 10);  // clamped
}

TEST_CASE("resolveMacroFloat empty bindings returns clamped localVal",
          "[macro]") {
  DummyNode n;
  CHECK(n.resolveMacroFloat(n.paramA, 0.5f, 0.f, 1.f) == 0.5f);
  CHECK(n.resolveMacroFloat(n.paramA, -0.1f, 0.f, 1.f) == 0.f);
  CHECK(n.resolveMacroFloat(n.paramA, 1.5f, 0.f, 1.f) == 1.f);
}

TEST_CASE("resolveMacroInt unipolar single binding", "[macro]") {
  DummyNode n;
  std::array<std::atomic<float>, 32> arr;
  fillAtomics(arr, 0.f);
  wireAtomics(n, arr);

  n.paramA.bindings.push_back({0, 1.0f});
  arr[0].store(0.5f);  // macroVal=0.5, intensity=1, range=10 → +5 → 0+5=5
  CHECK(n.resolveMacroInt(n.paramA, 0, 0, 10) == 5);

  arr[0].store(1.0f);  // +10 → clamped to 10
  CHECK(n.resolveMacroInt(n.paramA, 0, 0, 10) == 10);
}

TEST_CASE("resolveMacroInt bipolar single binding", "[macro]") {
  DummyNode n;
  std::array<std::atomic<float>, 32> arr;
  fillAtomics(arr, 0.f);
  wireAtomics(n, arr);
  n.macroBipolarMask.store(1u);  // macro 0 is bipolar

  n.paramA.bindings.push_back({0, 1.0f});
  arr[0].store(0.5f);  // (0.5-0.5)*2*1*10 = 0 → localVal 5 + 0 = 5
  CHECK(n.resolveMacroInt(n.paramA, 5, 0, 10) == 5);

  arr[0].store(1.0f);  // (1.0-0.5)*2*1*10 = 10 → 5+10 = 15 clamped to 10
  CHECK(n.resolveMacroInt(n.paramA, 5, 0, 10) == 10);

  arr[0].store(0.0f);  // (0.0-0.5)*2*1*10 = -10 → 5-10 = -5 clamped to 0
  CHECK(n.resolveMacroInt(n.paramA, 5, 0, 10) == 0);
}

TEST_CASE("resolveMacroInt multiple bindings sum additively", "[macro]") {
  DummyNode n;
  std::array<std::atomic<float>, 32> arr;
  fillAtomics(arr, 0.f);
  wireAtomics(n, arr);

  n.paramA.bindings.push_back({0, 0.5f});
  n.paramA.bindings.push_back({1, 0.5f});
  arr[0].store(0.5f);  // unipolar: 0.5*0.5*10 = 2.5
  arr[1].store(0.5f);  // unipolar: 0.5*0.5*10 = 2.5 → total 5
  CHECK(n.resolveMacroInt(n.paramA, 0, 0, 10) == 5);
}

TEST_CASE("resolveMacroInt null macro slot is ignored", "[macro]") {
  DummyNode n;
  // Don't wire macros — all slots remain null.
  n.paramA.bindings.push_back({3, 1.0f});  // points to null slot
  CHECK(n.resolveMacroInt(n.paramA, 7, 0, 10) == 7);
}

TEST_CASE("resolveMacroOffset is symmetric around zero", "[macro]") {
  DummyNode n;
  std::array<std::atomic<float>, 32> arr;
  fillAtomics(arr, 0.f);
  wireAtomics(n, arr);
  n.macroBipolarMask.store(1u);

  n.paramA.bindings.push_back({0, 1.0f});
  // resolveMacroOffset(param, localVal, maxVal) ≡ resolveMacroInt(param,
  // localVal, -maxVal, maxVal)
  arr[0].store(1.0f);  // bipolar full positive: (1-0.5)*2*1*24 = 24
  int via_offset = n.resolveMacroOffset(n.paramA, 0, 24);
  int via_int = n.resolveMacroInt(n.paramA, 0, -24, 24);
  CHECK(via_offset == via_int);
}
