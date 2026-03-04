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

    // Use screen area for positioning instead of holding a component pointer,
    // since the component may be destroyed by rebuild() before the async
    // callback fires.
    auto options = juce::PopupMenu::Options();
    if (targetComponent != nullptr)
      options =
          options.withTargetScreenArea(targetComponent->getScreenBounds());

    // Capture onMacroMapped by value (safe copy).
    // Do NOT capture targetComponent — it may be destroyed before callback.
    menu.showMenuAsync(options, [onMacroMapped](int result) {
      if (result == 0)
        return; // Dismissed

      if (result == 1) {
        if (onMacroMapped)
          onMacroMapped(-1);
      } else {
        int macroIndex = result - 2;
        if (onMacroMapped)
          onMacroMapped(macroIndex);
      }
    });
  }
};
