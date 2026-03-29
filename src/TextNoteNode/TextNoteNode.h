#pragma once

#include "../GraphNode.h"

class TextNoteNode : public GraphNode {
 public:
  TextNoteNode(int w, int h);
  ~TextNoteNode() override = default;

  [[nodiscard]] std::string getName() const override;

  // No ports — purely a visual/annotation node
  [[nodiscard]] int getNumInputPorts() const override { return 0; }
  [[nodiscard]] int getNumOutputPorts() const override { return 0; }

  [[nodiscard]] int getGridWidth() const override { return sizeW; }
  [[nodiscard]] int getGridHeight() const override { return sizeH; }

  [[nodiscard]] NodeLayout getLayout() const override;

  // No audio processing
  void process() override {}

  void saveNodeState(juce::XmlElement* xml) override;
  void loadNodeState(juce::XmlElement* xml) override;

  std::unique_ptr<juce::Component> createCustomComponent(
      const juce::String& name,
      juce::AudioProcessorValueTreeState* apvts = nullptr) override;

  juce::String noteText;

 private:
  int sizeW;
  int sizeH;
};
