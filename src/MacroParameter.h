#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Custom AudioParameterFloat that supports dynamic naming based on macro
// mappings. Always stays automatable (safe for VST3 hosts that cache metadata).
class MacroParameter : public juce::AudioParameterFloat {
 public:
  MacroParameter(int macroIndex)
      : juce::AudioParameterFloat(
            juce::ParameterID("macro_" + juce::String(macroIndex), 1),
            "Macro " + juce::String(macroIndex), 0.0f, 1.0f, 0.0f),
        index(macroIndex) {}

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

 private:
  int index;
  juce::String mappingName;
};
