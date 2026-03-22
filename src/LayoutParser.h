#pragma once

#include "NodeLayout.h"
#include <juce_core/juce_core.h>

class LayoutParser {
public:
  /// Parse a NodeLayout from a JSON byte buffer (e.g. BinaryData).
  /// Runtime pointers (valueRef, macroIndexRef) are left null;
  /// the caller must bind them after parsing.
  static NodeLayout parseFromJSON(const char *data, int size) {
    NodeLayout layout;

    auto jsonStr = juce::String::fromUTF8(data, size);
    auto parsed = juce::JSON::parse(jsonStr);

    if (parsed.isVoid())
      return layout;

    if (auto *obj = parsed.getDynamicObject()) {
      layout.gridWidth = obj->getProperty("gridWidth");
      layout.gridHeight = obj->getProperty("gridHeight");

      auto elementsVar = obj->getProperty("elements");
      if (auto *elementsArray = elementsVar.getArray()) {
        for (const auto &elemVar : *elementsArray) {
          if (auto *elemObj = elemVar.getDynamicObject()) {
            UIElement el;

            // Type
            juce::String typeStr = elemObj->getProperty("type").toString();
            el.type = parseType(typeStr);

            // Label
            if (elemObj->hasProperty("label"))
              el.label = elemObj->getProperty("label").toString();

            // Value range (for sliders)
            if (elemObj->hasProperty("minValue"))
              el.minValue = elemObj->getProperty("minValue");
            if (elemObj->hasProperty("maxValue"))
              el.maxValue = elemObj->getProperty("maxValue");

            // Options (for ComboBox)
            if (elemObj->hasProperty("options")) {
              auto optVar = elemObj->getProperty("options");
              if (auto *optArray = optVar.getArray()) {
                for (const auto &o : *optArray)
                  el.options.add(o.toString());
              }
            }

            // Custom type
            if (elemObj->hasProperty("customType"))
              el.customType = elemObj->getProperty("customType").toString();

            // Grid bounds [x, y, w, h]
            if (elemObj->hasProperty("gridBounds")) {
              auto boundsVar = elemObj->getProperty("gridBounds");
              if (auto *boundsArray = boundsVar.getArray()) {
                if (boundsArray->size() == 4) {
                  el.gridBounds = {(*boundsArray)[0], (*boundsArray)[1],
                                   (*boundsArray)[2], (*boundsArray)[3]};
                }
              }
            }

            layout.elements.push_back(el);
          }
        }
      }
    }

    return layout;
  }

private:
  static UIElementType parseType(const juce::String &typeStr) {
    if (typeStr == "RotarySlider")
      return UIElementType::RotarySlider;
    if (typeStr == "Toggle")
      return UIElementType::Toggle;
    if (typeStr == "Label")
      return UIElementType::Label;
    if (typeStr == "ComboBox")
      return UIElementType::ComboBox;
    if (typeStr == "PushButton")
      return UIElementType::PushButton;
    if (typeStr == "Custom")
      return UIElementType::Custom;

    // Default fallback
    return UIElementType::Label;
  }
};
