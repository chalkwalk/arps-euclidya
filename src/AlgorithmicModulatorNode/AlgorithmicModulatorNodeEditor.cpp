#include "../GraphCanvas.h"
#include "AlgorithmicModulatorNode.h"

class ModulatorStepEditor : public juce::Component, public juce::Timer {
 public:
  ModulatorStepEditor(AlgorithmicModulatorNode &node,
                      juce::AudioProcessorValueTreeState & /*apvts*/)
      : modNode(node) {
    startTimerHz(15);
    setSize(200, 80);
  }

  void paint(juce::Graphics &g) override {
    auto b = getLocalBounds();

    g.setColour(juce::Colour(0xff1a1a1a));
    g.fillRect(b);

    if (modNode.algorithm != AlgorithmicModulatorNode::Custom) {
      g.setColour(juce::Colours::darkgrey);
      g.setFont(10.0f);
      g.drawText("(Select Custom algorithm)", b, juce::Justification::centred);
      return;
    }

    int len = std::max(1, (int)modNode.customValues.size());
    float cellW = (float)b.getWidth() / (float)len;
    float bh = (float)b.getHeight();

    for (int i = 0; i < len; ++i) {
      float v = (i < (int)modNode.customValues.size())
                    ? std::clamp(modNode.customValues[(size_t)i], 0.0f, 1.0f)
                    : 0.5f;
      float barH = v * bh;
      auto barRect =
          juce::Rectangle<float>(b.getX() + (float)i * cellW,
                                 b.getY() + bh - barH, cellW - 1.0f, barH);

      g.setColour(juce::Colour(0xffaa44ff).withAlpha(0.55f + 0.45f * v));
      g.fillRect(barRect);

      g.setColour(juce::Colour(0xff333333));
      g.drawVerticalLine(b.getX() + (int)((float)(i + 1) * cellW),
                         (float)b.getY(), (float)b.getBottom());
    }

    g.setColour(juce::Colour(0xff444444));
    g.drawHorizontalLine(b.getCentreY(), (float)b.getX(), (float)b.getRight());
  }

  void mouseDown(const juce::MouseEvent &e) override { handleDrag(e); }
  void mouseDrag(const juce::MouseEvent &e) override { handleDrag(e); }

 private:
  void handleDrag(const juce::MouseEvent &e) {
    if (modNode.algorithm != AlgorithmicModulatorNode::Custom)
      return;

    auto b = getLocalBounds();
    int len = std::max(1, (int)modNode.customValues.size());
    float cellW = (float)b.getWidth() / (float)len;
    int col = (int)((float)(e.x - b.getX()) / cellW);

    if (col < 0 || col >= len)
      return;

    float v = 1.0f - std::clamp((float)(e.y - b.getY()) / (float)b.getHeight(),
                                0.0f, 1.0f);

    auto *canvas = findParentComponentOfClass<GraphCanvas>();
    if (canvas != nullptr) {
      canvas->performMutation([this, col, v]() {
        modNode.customValues[(size_t)col] = v;
        if (modNode.onNodeDirtied)
          modNode.onNodeDirtied();
      });
    } else {
      modNode.customValues[(size_t)col] = v;
      if (modNode.onNodeDirtied)
        modNode.onNodeDirtied();
    }
    repaint();
  }

  void timerCallback() override { repaint(); }

  AlgorithmicModulatorNode &modNode;
};

std::unique_ptr<juce::Component>
AlgorithmicModulatorNode::createCustomComponent(
    const juce::String &name, juce::AudioProcessorValueTreeState *apvts) {
  if (name == "ModulatorStepEditor" && apvts != nullptr) {
    return std::make_unique<ModulatorStepEditor>(*this, *apvts);
  }
  return nullptr;
}
