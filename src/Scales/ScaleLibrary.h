#pragma once

#include <juce_core/juce_core.h>

#include <vector>

#include "Scale.h"

class ScaleLibrary {
 public:
  static ScaleLibrary &getInstance() {
    static ScaleLibrary instance;
    return instance;
  }

  // Returns all scale names for the given tuning (factory then user), in order.
  [[nodiscard]] std::vector<juce::String> getScaleNames(
      const juce::String &tuningName) const;

  // Find a scale by name + tuning. Returns nullptr if not found.
  [[nodiscard]] const Scale *findScale(const juce::String &name,
                                      const juce::String &tuningName) const;

  // Write a user scale to disk and add it to the in-memory library.
  void saveUserScale(const Scale &scale);

  // Re-scan the user scale directory for the given tuning.
  void scanUserScales(const juce::String &tuningName);

  static juce::File getUserScaleDirectory(const juce::String &tuningName);

 private:
  ScaleLibrary();

  static std::vector<Scale> loadFactoryScales();
  static Scale parseScaleJSON(const juce::String &json);

  std::vector<Scale> factoryScales;
  std::vector<Scale> userScales;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ScaleLibrary)
};
