#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>

#include "GraphNode.h"
#include "SharedMacroUI.h"

struct SliderMacroInfo {
  juce::Slider *slider;
  MacroParam *macroParamRef;
};

struct ButtonMacroInfo {
  CustomMacroButton *button;
  MacroParam *macroParamRef;
  int *valueRef;
};

struct ComboMacroInfo {
  CustomMacroComboBox *combo;
  MacroParam *macroParamRef;
  int *valueRef;
};

namespace MacroOverlay {
void paint(juce::Graphics &g, GraphNode *node,
           const std::vector<SliderMacroInfo> &sliderInfos,
           const std::vector<ButtonMacroInfo> &buttonInfos,
           const std::vector<ComboMacroInfo> &comboInfos, int selectedMacro,
           int highlightedMacro);
}
