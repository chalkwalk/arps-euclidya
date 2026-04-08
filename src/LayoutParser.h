#pragma once

#include <juce_core/juce_core.h>

#include "NodeLayout.h"

class LayoutParser {
 public:
  /// Parse a NodeLayout from a JSON byte buffer (e.g. BinaryData).
  /// Runtime pointers (valueRef, macroParamRef) are left null;
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

      if (obj->hasProperty("extendedGridWidth"))
        layout.extendedGridWidth = obj->getProperty("extendedGridWidth");
      if (obj->hasProperty("extendedGridHeight"))
        layout.extendedGridHeight = obj->getProperty("extendedGridHeight");

      auto elementsVar = obj->getProperty("elements");
      if (auto *elementsArray = elementsVar.getArray()) {
        for (const auto &elemVar : *elementsArray) {
          if (auto *elemObj = elemVar.getDynamicObject()) {
            layout.elements.push_back(parseElement(elemObj));
          }
        }
      }

      auto extendedVar = obj->getProperty("extendedElements");
      if (auto *extendedArray = extendedVar.getArray()) {
        for (const auto &elemVar : *extendedArray) {
          if (auto *elemObj = elemVar.getDynamicObject()) {
            layout.extendedElements.push_back(parseElement(elemObj));
          }
        }
      }
    }

    return layout;
  }

 private:
  static UIElement parseElement(juce::DynamicObject *elemObj) {
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

    // Float range (for float-valued sliders)
    if (elemObj->hasProperty("floatMin"))
      el.floatMin = (float)(double)elemObj->getProperty("floatMin");
    if (elemObj->hasProperty("floatMax"))
      el.floatMax = (float)(double)elemObj->getProperty("floatMax");

    // Step size
    if (elemObj->hasProperty("step"))
      el.step = (float)(double)elemObj->getProperty("step");

    // Bipolar flag
    if (elemObj->hasProperty("bipolar"))
      el.bipolar = (bool)elemObj->getProperty("bipolar");

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

    // Label text color
    if (elemObj->hasProperty("colorHex"))
      el.colorHex = elemObj->getProperty("colorHex").toString();

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

    return el;
  }
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
