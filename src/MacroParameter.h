#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Custom AudioParameterFloat that supports dynamic naming based on macro
// mappings. Always stays automatable (safe for VST3 hosts that cache metadata).
class MacroParameter : public juce::AudioParameterFloat {
 public:
  // macroIndex is 1-based (1–32). Macros 17–32 (UI indices 16–31) default
  // to bipolar so the upper half of the palette works center-zero out of the box.
  MacroParameter(int macroIndex)
      : juce::AudioParameterFloat(
            juce::ParameterID("macro_" + juce::String(macroIndex), 1),
            "Macro " + juce::String(macroIndex), 0.0f, 1.0f, 0.0f),
        index(macroIndex),
        bipolar(macroIndex > 16) {}

  juce::String getName(int maximumStringLength) const override {
    juce::String name;
    if (mappingName.isEmpty()) {
      name = "Macro " + juce::String(index);
    } else {
      name = juce::String(index) + " - " + mappingName;
    }
    return name.substring(0, maximumStringLength);
  }

  void setMappingName(const juce::String &name) { mappingName = name; }
  void clearMapping() { mappingName = ""; }

  bool isMapped() const { return mappingName.isNotEmpty(); }

  int getMacroIndex() const { return index; }

  void toggleBipolar() { bipolar = !bipolar; }
  void setBipolar(bool b) { bipolar = b; }
  bool isBipolar() const { return bipolar; }

  // MIDI learn: CC source for this macro. -1 = none.
  int learnedCC = -1;
  int learnedChannel = -1;

  bool hasLearnedCC() const { return learnedCC >= 0; }
  void clearLearnedCC() { learnedCC = -1; learnedChannel = -1; }

 private:
  int index;
  juce::String mappingName;
  bool bipolar = false;
};
