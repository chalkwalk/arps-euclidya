#include "ScaleEditorComponent.h"

#include "../QuantizerNode/QuantizerNode.h"
#include "HarmonicAnalysis.h"
#include "ScaleLibrary.h"

static constexpr int kButtonH = 24;
static constexpr int kCellPad = 3;

ScaleEditorComponent::ScaleEditorComponent(QuantizerNode &n) : node(n) {
  addAndMakeVisible(saveAsButton);
  saveAsButton.onClick = [this] { saveAs(); };
  startTimerHz(20);
}

juce::Rectangle<int> ScaleEditorComponent::gridArea() const {
  return getLocalBounds().withTrimmedBottom(kButtonH + kCellPad);
}

int ScaleEditorComponent::cellAtPoint(juce::Point<int> p) const {
  const int steps = node.stepsPerOctave.load();
  if (steps <= 0) {
    return -1;
  }
  auto area = gridArea();
  if (!area.contains(p)) {
    return -1;
  }
  float cellW = static_cast<float>(area.getWidth()) / static_cast<float>(steps);
  int cell = static_cast<int>(static_cast<float>(p.x - area.getX()) / cellW);
  return juce::jlimit(0, steps - 1, cell);
}

void ScaleEditorComponent::paint(juce::Graphics &g) {
  g.fillAll(juce::Colour(0xff1a1a1a));

  const int steps = node.stepsPerOctave.load();
  if (steps <= 0) {
    return;
  }
  const uint32_t mask = node.stepMaskBits.load();
  const TuningTable *tuning = node.getActiveTuning();

  auto area = gridArea();
  float cellW = static_cast<float>(area.getWidth()) / static_cast<float>(steps);
  float cellH = static_cast<float>(area.getHeight());

  for (int i = 0; i < steps; ++i) {
    float x = static_cast<float>(area.getX()) + (static_cast<float>(i) * cellW);
    auto cellRect = juce::Rectangle<float>(x + 1.0f, static_cast<float>(area.getY()) + 1.0f,
                                           cellW - 2.0f, cellH - 2.0f);

    float score = HarmonicAnalysis::scoreStep(tuning, steps, 0, i);
    bool active = (mask >> i) & 1u;
    bool hovered = (i == hoveredCell);

    // Inactive background: warm amber tint scaled by consonance (clearly readable)
    g.setColour(juce::Colour::fromFloatRGBA(1.0f, 0.72f, 0.18f, 0.04f + score * 0.32f));
    g.fillRoundedRectangle(cellRect, 2.0f);

    // Active fill
    if (active) {
      g.setColour(juce::Colour(0xff44aaff).withAlpha(0.78f));
      g.fillRoundedRectangle(cellRect, 2.0f);
    }

    // Consonance accent strip at the bottom of every cell, visible on both
    // active (blue) and inactive backgrounds.
    float accentH = std::min(4.0f, cellRect.getHeight() * 0.14f);
    auto accentRect = juce::Rectangle<float>(
        cellRect.getX() + 1.0f, cellRect.getBottom() - accentH,
        cellRect.getWidth() - 2.0f, accentH);
    g.setColour(juce::Colour::fromFloatRGBA(1.0f, 0.85f, 0.2f, score * 0.9f));
    g.fillRoundedRectangle(accentRect, 1.0f);

    // Hover ring
    if (hovered) {
      g.setColour(juce::Colours::white.withAlpha(0.5f));
      g.drawRoundedRectangle(cellRect, 2.0f, 1.5f);
    }

    // Step number label (root = bold)
    if (i == 0) {
      g.setColour(active ? juce::Colours::white : juce::Colour(0xff999999));
      g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    } else {
      g.setColour(active ? juce::Colours::white.withAlpha(0.85f)
                         : juce::Colour(0xff606060));
      g.setFont(juce::FontOptions(9.0f));
    }
    g.drawText(juce::String(i), cellRect.toNearestInt(),
               juce::Justification::centred, false);
  }

  // Scale name label
  g.setColour(juce::Colour(0xff888888));
  g.setFont(juce::FontOptions(10.0f));
  g.drawText(node.selectedScaleName,
             getLocalBounds().removeFromBottom(kButtonH + kCellPad)
                 .withTrimmedRight(saveAsButton.getWidth() + kCellPad),
             juce::Justification::centredLeft, true);
}

void ScaleEditorComponent::resized() {
  auto bottom = getLocalBounds().removeFromBottom(kButtonH).reduced(kCellPad, 2);
  saveAsButton.setBounds(bottom.removeFromRight(80));
}

void ScaleEditorComponent::mouseDown(const juce::MouseEvent &e) {
  int cell = cellAtPoint(e.getPosition());
  if (cell < 0) {
    return;
  }
  uint32_t mask = node.stepMaskBits.load();
  mask ^= (1u << cell);
  node.stepMaskBits.store(mask);
  // Clear the named scale — mask is now a custom edit.
  // Do NOT call parameterChanged(): that reloads stepMaskBits from the library,
  // which would immediately undo the toggle.
  node.selectedScaleName = "Custom";
  repaint();
}

void ScaleEditorComponent::mouseMove(const juce::MouseEvent &e) {
  int cell = cellAtPoint(e.getPosition());
  if (cell != hoveredCell) {
    hoveredCell = cell;
    repaint();
  }
}

void ScaleEditorComponent::mouseExit(const juce::MouseEvent & /*e*/) {
  if (hoveredCell >= 0) {
    hoveredCell = -1;
    repaint();
  }
}

void ScaleEditorComponent::timerCallback() {
  const uint32_t mask = node.stepMaskBits.load();
  const int steps = node.stepsPerOctave.load();
  if (mask != lastKnownMask || steps != lastKnownSteps) {
    lastKnownMask = mask;
    lastKnownSteps = steps;
    repaint();
  }
}

void ScaleEditorComponent::saveAs() {
  auto *dialog = new juce::AlertWindow("Save Scale", "Enter a name for this scale:",
                                       juce::MessageBoxIconType::NoIcon);
  dialog->addTextEditor("name", node.selectedScaleName);
  dialog->addButton("Save", 1);
  dialog->addButton("Cancel", 0);
  dialog->enterModalState(
      true,
      juce::ModalCallbackFunction::create([this, dialog](int result) {
        if (result == 1) {
          juce::String name = dialog->getTextEditorContents("name").trim();
          if (name.isNotEmpty()) {
            const TuningTable *tuning = node.getActiveTuning();
            juce::String tuningName = tuning ? tuning->name : "";
            const int steps = node.stepsPerOctave.load();
            const uint32_t bits = node.stepMaskBits.load();
            Scale scale;
            scale.name = name;
            scale.tuningName = tuningName;
            for (int i = 0; i < steps; ++i) {
              scale.stepMask.push_back((bits >> i) & 1u);
            }
            ScaleLibrary::getInstance().saveUserScale(scale);
            node.selectedScaleName = name;
            repaint();
          }
        }
      }),
      true);
}
