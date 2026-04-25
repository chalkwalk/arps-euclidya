#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>

enum class UIElementType : uint8_t {
  RotarySlider,
  Toggle,
  Label,
  ComboBox,
  PushButton,
  Custom
};

struct MacroParam;

struct UIElement {
  UIElementType type;
  juce::String label;
  int *valueRef = nullptr;
  MacroParam *macroParamRef = nullptr;
  int minValue = 0;
  int maxValue = 0;
  int *dynamicMinRef = nullptr;    // Dynamic min bound
  int *dynamicMaxRef = nullptr;    // Dynamic max bound
  float *floatValueRef = nullptr;  // For float-valued sliders
  float floatMin = 0.0f;
  float floatMax = 1.0f;
  float step = 1.0f;          // Slider step (1 for int, 0.01 for float, etc.)
  bool bipolar = false;       // Centered-zero slider
  juce::String colorHex;      // Text color code (e.g. "ffff00ff")
  juce::StringArray options;  // For ComboBox
  juce::String customType;    // For Custom components
  juce::Rectangle<int> gridBounds;
};

// Per-direction unfold extension counts. Defaults to 1 in all directions.
struct UnfoldExtents {
  int up = 0, down = 0, left = 0, right = 0;
};

struct NodeLayout {
  int gridWidth = 1;
  int gridHeight = 1;

  // ── Compact state ───────────────────────────────────────────────────────────
  // All compact tab elements, flattened in tab order.
  // Single unnamed tab ("") = flat view with no visible tab bar — backward compat.
  std::vector<UIElement> elements;
  juce::StringArray compactTabLabels;   // one entry per tab; size 0 or 1 = no bar
  std::vector<int> compactTabBoundaries; // [t] = first index of tab t in elements

  // ── Unfold state (optional) ─────────────────────────────────────────────────
  // Overlays neighbours; asymmetric per-direction extents.
  std::vector<UIElement> unfoldedElements;
  UnfoldExtents unfoldExtents;
  juce::StringArray unfoldedTabLabels;
  std::vector<int> unfoldedTabBoundaries;

  // ── Queries ─────────────────────────────────────────────────────────────────
  [[nodiscard]] bool hasCompactTabs()    const { return compactTabLabels.size() > 1; }
  [[nodiscard]] bool hasUnfoldedLayout() const { return !unfoldedElements.empty(); }
  [[nodiscard]] bool hasUnfoldedTabs()  const { return unfoldedTabLabels.size() > 1; }

  // Element range {start, count} for a specific tab in each section.
  [[nodiscard]] std::pair<int, int> compactTabRange(int t) const {
    return tabRange(compactTabBoundaries, (int)elements.size(), t);
  }
  [[nodiscard]] std::pair<int, int> unfoldedTabElementRange(int t) const {
    return tabRange(unfoldedTabBoundaries, (int)unfoldedElements.size(), t);
  }

 private:
  [[nodiscard]] static std::pair<int, int> tabRange(const std::vector<int> &bounds,
                                                    int total, int t) {
    if (bounds.empty() || t >= (int)bounds.size())
      return {0, total};
    int start = bounds[(size_t)t];
    int end = (t + 1 < (int)bounds.size()) ? bounds[(size_t)(t + 1)] : total;
    return {start, end - start};
  }
};
