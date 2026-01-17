#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class MacroMappingMenu {
public:
  static void showMenu(juce::Component *targetComponent, int currentMappedMacro,
                       std::function<void(int)> onMacroMapped) {
    juce::PopupMenu menu;
    menu.addItem(1, "Clear Mapping", true, currentMappedMacro == -1);
    menu.addSeparator();

    for (int i = 1; i <= 32; ++i) {
      menu.addItem(i + 1, "Map to Macro " + juce::String(i), true,
                   currentMappedMacro == (i - 1));
    }

    menu.showMenuAsync(
        juce::PopupMenu::Options().withTargetComponent(targetComponent),
        [targetComponent, onMacroMapped](int result) {
          if (result == 0)
            return; // Dismissed

          if (result == 1) {
            if (auto *tc = dynamic_cast<juce::SettableTooltipClient *>(
                    targetComponent))
              tc->setTooltip("");
            if (onMacroMapped)
              onMacroMapped(-1);
          } else {
            int macroIndex = result - 2;
            if (auto *tc = dynamic_cast<juce::SettableTooltipClient *>(
                    targetComponent))
              tc->setTooltip("Mapped to Macro " + juce::String(macroIndex + 1));
            if (onMacroMapped)
              onMacroMapped(macroIndex);
          }
        });
  }
};
