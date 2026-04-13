#pragma once

#include "../GraphNode.h"

// Splits notes into two streams by velocity at graph process() time.
// Output 0 ("≥"): notes where velocity >= threshold.
// Output 1 ("<"): notes where velocity <  threshold.
// Empty steps produce rests on both outputs.
class VelocityFilterNode : public GraphNode {
 public:
  VelocityFilterNode() = default;
  ~VelocityFilterNode() override = default;

  [[nodiscard]] std::string getName() const override {
    return "Velocity Filter";
  }
  [[nodiscard]] int getNumInputPorts() const override { return 1; }
  [[nodiscard]] int getNumOutputPorts() const override { return 2; }

  void process() override;

  [[nodiscard]] NodeLayout getLayout() const override;

  void saveNodeState(juce::XmlElement *xml) override;
  void loadNodeState(juce::XmlElement *xml) override;

  // Split point in [0.0, 1.0]. Notes with velocity >= threshold → output 0.
  float threshold = 0.5f;
};
