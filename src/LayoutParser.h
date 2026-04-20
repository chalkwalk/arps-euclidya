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

    auto *obj = parsed.getDynamicObject();
    if (obj == nullptr)
      return layout;

    layout.gridWidth = obj->getProperty("gridWidth");
    layout.gridHeight = obj->getProperty("gridHeight");

    // Tab-expanded dimensions
    if (obj->hasProperty("expandedGridWidth"))
      layout.expandedGridWidth = obj->getProperty("expandedGridWidth");
    if (obj->hasProperty("expandedGridHeight"))
      layout.expandedGridHeight = obj->getProperty("expandedGridHeight");

    // Unfold extents (per-direction; defaults to 1 if absent)
    if (obj->hasProperty("unfoldExtents")) {
      if (auto *ex = obj->getProperty("unfoldExtents").getDynamicObject()) {
        if (ex->hasProperty("up"))    layout.unfoldExtents.up    = ex->getProperty("up");
        if (ex->hasProperty("down"))  layout.unfoldExtents.down  = ex->getProperty("down");
        if (ex->hasProperty("left"))  layout.unfoldExtents.left  = ex->getProperty("left");
        if (ex->hasProperty("right")) layout.unfoldExtents.right = ex->getProperty("right");
      }
    }

    // ── Compact elements ────────────────────────────────────────────────────
    // New format: "tabs" array of {label, elements[]}
    // Legacy format: "elements" flat array  →  wraps as single unnamed tab
    if (obj->hasProperty("tabs")) {
      parseTabs(obj->getProperty("tabs"), layout.elements,
                layout.compactTabLabels, layout.compactTabBoundaries);
    } else {
      auto elemVar = obj->getProperty("elements");
      if (auto *arr = elemVar.getArray()) {
        layout.compactTabLabels.add("");
        layout.compactTabBoundaries.push_back(0);
        for (const auto &v : *arr)
          if (auto *e = v.getDynamicObject())
            layout.elements.push_back(parseElement(e));
      }
    }

    // ── Expanded elements ───────────────────────────────────────────────────
    if (obj->hasProperty("expandedTabs")) {
      parseTabs(obj->getProperty("expandedTabs"), layout.expandedElements,
                layout.expandedTabLabels, layout.expandedTabBoundaries);
    }

    // ── Unfolded elements ───────────────────────────────────────────────────
    // New format: "unfoldedTabs" array
    // Legacy formats: "unfoldedElements" or "extendedElements" flat arrays
    if (obj->hasProperty("unfoldedTabs")) {
      parseTabs(obj->getProperty("unfoldedTabs"), layout.unfoldedElements,
                layout.unfoldedTabLabels, layout.unfoldedTabBoundaries);
    } else {
      auto unfoldedVar = obj->hasProperty("unfoldedElements")
                             ? obj->getProperty("unfoldedElements")
                             : obj->getProperty("extendedElements");
      if (auto *arr = unfoldedVar.getArray()) {
        layout.unfoldedTabLabels.add("");
        layout.unfoldedTabBoundaries.push_back(0);
        for (const auto &v : *arr)
          if (auto *e = v.getDynamicObject())
            layout.unfoldedElements.push_back(parseElement(e));
      }
    }

    return layout;
  }

 private:
  // Parse an array of {label, elements[]} objects and flatten into out_elements.
  // Populates labels and boundaries as it goes.
  static void parseTabs(const juce::var &tabsVar,
                        std::vector<UIElement> &out_elements,
                        juce::StringArray &out_labels,
                        std::vector<int> &out_boundaries) {
    auto *arr = tabsVar.getArray();
    if (arr == nullptr)
      return;
    for (const auto &tabVar : *arr) {
      auto *tabObj = tabVar.getDynamicObject();
      if (tabObj == nullptr)
        continue;
      out_labels.add(tabObj->getProperty("label").toString());
      out_boundaries.push_back((int)out_elements.size());
      auto elemVar = tabObj->getProperty("elements");
      if (auto *elemsArr = elemVar.getArray()) {
        for (const auto &v : *elemsArr)
          if (auto *e = v.getDynamicObject())
            out_elements.push_back(parseElement(e));
      }
    }
  }

  static UIElement parseElement(juce::DynamicObject *elemObj) {
    UIElement el;

    juce::String typeStr = elemObj->getProperty("type").toString();
    el.type = parseType(typeStr);

    if (elemObj->hasProperty("label"))
      el.label = elemObj->getProperty("label").toString();
    if (elemObj->hasProperty("minValue"))
      el.minValue = elemObj->getProperty("minValue");
    if (elemObj->hasProperty("maxValue"))
      el.maxValue = elemObj->getProperty("maxValue");
    if (elemObj->hasProperty("floatMin"))
      el.floatMin = (float)(double)elemObj->getProperty("floatMin");
    if (elemObj->hasProperty("floatMax"))
      el.floatMax = (float)(double)elemObj->getProperty("floatMax");
    if (elemObj->hasProperty("step"))
      el.step = (float)(double)elemObj->getProperty("step");
    if (elemObj->hasProperty("bipolar"))
      el.bipolar = (bool)elemObj->getProperty("bipolar");
    if (elemObj->hasProperty("options")) {
      auto optVar = elemObj->getProperty("options");
      if (auto *optArray = optVar.getArray())
        for (const auto &o : *optArray)
          el.options.add(o.toString());
    }
    if (elemObj->hasProperty("customType"))
      el.customType = elemObj->getProperty("customType").toString();
    if (elemObj->hasProperty("colorHex"))
      el.colorHex = elemObj->getProperty("colorHex").toString();
    if (elemObj->hasProperty("gridBounds")) {
      auto boundsVar = elemObj->getProperty("gridBounds");
      if (auto *boundsArray = boundsVar.getArray()) {
        if (boundsArray->size() == 4)
          el.gridBounds = {(*boundsArray)[0], (*boundsArray)[1],
                           (*boundsArray)[2], (*boundsArray)[3]};
      }
    }

    return el;
  }

  static UIElementType parseType(const juce::String &typeStr) {
    if (typeStr == "RotarySlider") return UIElementType::RotarySlider;
    if (typeStr == "Toggle")       return UIElementType::Toggle;
    if (typeStr == "Label")        return UIElementType::Label;
    if (typeStr == "ComboBox")     return UIElementType::ComboBox;
    if (typeStr == "PushButton")   return UIElementType::PushButton;
    if (typeStr == "Custom")       return UIElementType::Custom;
    return UIElementType::Label;
  }
};
