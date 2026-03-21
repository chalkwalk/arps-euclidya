#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

enum class UIElementType {
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
  juce::StringArray options; // For ComboBox
  juce::String customType;   // For Custom components
  juce::Rectangle<int> gridBounds;
};

struct NodeLayout {
  int gridWidth = 1;
  int gridHeight = 1;
  std::vector<UIElement> elements;
};
