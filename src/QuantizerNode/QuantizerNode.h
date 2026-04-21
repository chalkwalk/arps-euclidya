#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <atomic>

#include "../GraphNode.h"
#include "../Tuning/TuningTable.h"

class QuantizerNode : public GraphNode {
 public:
  QuantizerNode();
  ~QuantizerNode() override = default;

  void process() override;
  NodeLayout getLayout() const override;
  std::unique_ptr<juce::Component> createCustomComponent(
      const juce::String &name,
      juce::AudioProcessorValueTreeState *apvts = nullptr) override;

  std::string getName() const override { return "Quantizer"; }

  void saveNodeState(juce::XmlElement *xml) override;
  void loadNodeState(juce::XmlElement *xml) override;
  void parameterChanged() override;

  void setActiveTuning(const TuningTable *t);
  [[nodiscard]] const TuningTable *getActiveTuning() const { return activeTuning; }

  // Primary state
  juce::String selectedScaleName = "Ionian";
  int rootNote = 0;
  int mode = 0;
  int restOnDrop = 1;

  // Thread-safe step mask: bit i = 1 means step i is in the scale.
  // Updated on the message thread; read atomically on the audio thread.
  std::atomic<uint32_t> stepMaskBits{0b101101010101};  // Ionian
  std::atomic<int> stepsPerOctave{12};

  MacroParam macroRootNote{"Root Note", {}};
  MacroParam macroMode{};
  MacroParam macroRestOnDrop{};

 private:
  const TuningTable *activeTuning = nullptr;
  mutable int scaleIndex = 0;  // combo-box binding (updated in getLayout)

  void refreshStepMask();
};
