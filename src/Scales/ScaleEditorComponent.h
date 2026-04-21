#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class QuantizerNode;

class ScaleEditorComponent : public juce::Component, private juce::Timer {
 public:
  explicit ScaleEditorComponent(QuantizerNode &node);
  ~ScaleEditorComponent() override = default;

  void paint(juce::Graphics &g) override;
  void resized() override;
  void mouseDown(const juce::MouseEvent &e) override;
  void mouseMove(const juce::MouseEvent &e) override;
  void mouseExit(const juce::MouseEvent &e) override;

 private:
  void timerCallback() override;
  void saveAs();

  QuantizerNode &node;
  juce::TextButton saveAsButton{"Save as..."};

  int hoveredCell = -1;
  uint32_t lastKnownMask = 0;
  int lastKnownSteps = 0;

  [[nodiscard]] juce::Rectangle<int> gridArea() const;
  [[nodiscard]] int cellAtPoint(juce::Point<int> p) const;
};
