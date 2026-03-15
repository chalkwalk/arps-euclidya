#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

class EuclideanVisualizer : public juce::Component {
public:
  EuclideanVisualizer() = default;

  void update(const std::vector<bool> &pattern, int patternIdx,
              const std::vector<bool> &rhythm, int rhythmIdx) {
    currentPattern = pattern;
    currentPatternIdx = patternIdx;
    currentRhythm = rhythm;
    currentRhythmIdx = rhythmIdx;
    repaint();
  }

  void paint(juce::Graphics &g) override {
    auto bounds = getLocalBounds().toFloat().reduced(10.0f);
    auto center = bounds.getCentre();
    float radius = std::min(bounds.getWidth(), bounds.getHeight()) * 0.5f;

    // Draw Rhythm Ring (Outer)
    drawRing(g, center, radius, 10.0f, currentRhythm, juce::Colours::cyan);

    // Draw Pattern Ring (Inner)
    drawRing(g, center, radius - 16.0f, 6.0f, currentPattern,
             juce::Colours::magenta);

    // Draw Playheads on top for clarity
    drawPlayhead(g, center, radius, 10.0f, (int)currentRhythm.size(),
                 currentRhythmIdx, juce::Colours::cyan);
    drawPlayhead(g, center, radius - 16.0f, 6.0f, (int)currentPattern.size(),
                 currentPatternIdx, juce::Colours::magenta);
  }

private:
  void drawRing(juce::Graphics &g, juce::Point<float> center, float radius,
                float thickness, const std::vector<bool> &pattern,
                juce::Colour baseColor) {
    if (pattern.empty())
      return;

    int numSteps = (int)pattern.size();
    float angleStep = juce::MathConstants<float>::twoPi / (float)numSteps;
    float gap = std::max(
        0.015f, 0.12f / (float)numSteps); // Dynamic gap to avoid overlaps
    float startAngle = -juce::MathConstants<float>::halfPi;

    for (int i = 0; i < numSteps; ++i) {
      float angle = startAngle + (float)i * angleStep;

      juce::Path p;
      p.addCentredArc(center.x, center.y, radius, radius, 0.0f, angle + gap,
                      angle + angleStep - gap, true);

      bool isBeat = pattern[i];
      auto color = baseColor.withAlpha(isBeat ? 0.6f : 0.05f);

      if (isBeat) {
        // Outer glow for beat
        g.setColour(baseColor.withAlpha(0.2f));
        g.strokePath(p, juce::PathStrokeType(thickness + 3.0f,
                                             juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));
      }

      g.setColour(color);
      g.strokePath(p,
                   juce::PathStrokeType(thickness, juce::PathStrokeType::curved,
                                        juce::PathStrokeType::rounded));
    }
  }

  void drawPlayhead(juce::Graphics &g, juce::Point<float> center, float radius,
                    float thickness, int numSteps, int activeIdx,
                    juce::Colour baseColor) {
    if (numSteps <= 0 || activeIdx < 0)
      return;

    float angleStep = juce::MathConstants<float>::twoPi / (float)numSteps;
    float angle =
        -juce::MathConstants<float>::halfPi + (float)activeIdx * angleStep;
    float gap = std::max(0.01f, 0.08f / (float)numSteps);

    juce::Path p;
    p.addCentredArc(center.x, center.y, radius, radius, 0.0f, angle + gap,
                    angle + angleStep - gap, true);

    // Bright white indicator for playhead
    g.setColour(juce::Colours::white);
    g.strokePath(p,
                 juce::PathStrokeType(thickness, juce::PathStrokeType::curved,
                                      juce::PathStrokeType::rounded));

    // Bright glow
    g.setColour(juce::Colours::white.withAlpha(0.4f));
    g.strokePath(p, juce::PathStrokeType(thickness + 2.0f,
                                         juce::PathStrokeType::curved,
                                         juce::PathStrokeType::rounded));
  }

  std::vector<bool> currentPattern;
  int currentPatternIdx = -1;
  std::vector<bool> currentRhythm;
  int currentRhythmIdx = -1;
};
