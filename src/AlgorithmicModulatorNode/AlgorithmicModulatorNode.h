#pragma once

#include <vector>

#include "../GraphNode.h"

class AlgorithmicModulatorNode : public GraphNode {
 public:
  enum Algorithm {
    Sine = 0,
    RampUp,
    RampDown,
    Triangle,
    RandomHold,
    RandomWalk,
    EuclideanGates,
    Custom
  };

  AlgorithmicModulatorNode();
  ~AlgorithmicModulatorNode() override = default;

  std::string getName() const override { return "CC Modulator"; }
  int getNumInputPorts() const override { return 0; }
  int getNumOutputPorts() const override { return 1; }
  PortType getOutputPortType(int /*port*/) const override { return PortType::CC; }

  void process() override;
  NodeLayout getLayout() const override;
  std::unique_ptr<juce::Component> createCustomComponent(
      const juce::String &name,
      juce::AudioProcessorValueTreeState *apvts = nullptr) override;

  void saveNodeState(juce::XmlElement *xml) override;
  void loadNodeState(juce::XmlElement *xml) override;

  // --- Controls ---
  int algorithm = 0;      // Algorithm enum value
  int ccNumber = 1;       // 0..127  (Mod Wheel default)
  int length = 8;         // 1..64
  float rangeMin = 0.0f;  // normalised 0..1
  float rangeMax = 1.0f;  // normalised 0..1
  int steps = 8;          // 1..64  (Euclidean)
  int beats = 4;          // 1..64  (Euclidean)
  int offset = 0;         // 0..63  (Euclidean)

  // Per-step values for Custom algorithm (length-sized, 0..1)
  std::vector<float> customValues;

  // --- Macro params ---
  MacroParam macroCCNumber{"CC Number", {}};
  MacroParam macroLength{"Length", {}};
  MacroParam macroRangeMin{"Range Min", {}};
  MacroParam macroRangeMax{"Range Max", {}};
  MacroParam macroSteps{"Steps", {}};
  MacroParam macroBeats{"Beats", {}};
  MacroParam macroOffset{"Offset", {}};

 private:
  // Ensure customValues has exactly `length` entries (default 0.5)
  void ensureCustomValues();
};
