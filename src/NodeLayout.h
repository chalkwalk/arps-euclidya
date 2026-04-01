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

struct UIElement {
  UIElementType type;
  juce::String label;
  int *valueRef = nullptr;
  int *macroIndexRef = nullptr;
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

struct NodeLayout {
  int gridWidth = 1;
  int gridHeight = 1;

  // For "Origami" extended UI
  int extendedGridWidth = 0;   // 0 means no extended UI
  int extendedGridHeight = 0;  // 0 means no extended UI

  std::vector<UIElement> elements;
  std::vector<UIElement> extendedElements;
};
