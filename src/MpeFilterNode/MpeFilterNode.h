#pragma once

#include "../GraphNode.h"

// Splits every note into two streams based on a live MPE axis value.
// Output 0 ("≥"): notes whose condition passes at MidiOutNode playback time.
// Output 1 ("<"): notes whose condition fails.
// Both outputs carry every note from the input; the MpeCondition on each copy
// determines which one will actually fire on a given tick.
class MpeFilterNode : public GraphNode {
 public:
  MpeFilterNode() { addMacroParam(&macroAxis); }
  ~MpeFilterNode() override = default;

  [[nodiscard]] std::string getName() const override { return "MPE Filter"; }
  [[nodiscard]] int getNumInputPorts() const override { return 1; }
  [[nodiscard]] int getNumOutputPorts() const override { return 2; }

  void process() override;

  [[nodiscard]] NodeLayout getLayout() const override;

  void saveNodeState(juce::XmlElement *xml) override;
  void loadNodeState(juce::XmlElement *xml) override;

  // 0 = Bend (X), 1 = Timbre (Y), 2 = Pressure (Z)
  int axis = 0;

  // Split point in [0.0, 1.0].
  // For Bend: 0.5 = centre (no bend). Stored normalised; UI shows ±%.
  float threshold = 0.5f;

  MacroParam macroAxis{"MPE Axis", {}};
};
