#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>

class EuclideanVisualizer : public juce::Component {
 public:
  EuclideanVisualizer() = default;

  void update(const std::vector<bool> &pattern, int patternIdx,
              const std::vector<bool> &rhythm, int rhythmIdx, bool notePlayed) {
    if (pattern != currentPattern || rhythm != currentRhythm) {
      currentPattern = pattern;
      currentRhythm = rhythm;
      pathsDirty = true;
    }

    if (patternIdx != currentPatternIdx || rhythmIdx != currentRhythmIdx ||
        notePlayed != currentNotePlayed) {
      currentPatternIdx = patternIdx;
      currentRhythmIdx = rhythmIdx;
      currentNotePlayed = notePlayed;
      repaint();
    }
  }

  void paint(juce::Graphics &g) override {
    auto bounds = getLocalBounds().toFloat().reduced(10.0f);
    auto center = bounds.getCentre();
    float radius = std::min(bounds.getWidth(), bounds.getHeight()) * 0.5f;

    if (pathsDirty) {
      rebuildPaths(center, radius);
      pathsDirty = false;
    }

    // --- Draw Rings (Cached) ---
    // Outer Rhythm
    g.setColour(juce::Colours::cyan.withAlpha(0.05f));
    g.strokePath(rhythmRestPath,
                 juce::PathStrokeType(10.0f, juce::PathStrokeType::curved,
                                      juce::PathStrokeType::rounded));

    g.setColour(juce::Colours::cyan.withAlpha(0.2f));
    g.strokePath(rhythmBeatPath,
                 juce::PathStrokeType(13.0f, juce::PathStrokeType::curved,
                                      juce::PathStrokeType::rounded));
    g.setColour(juce::Colours::cyan.withAlpha(0.6f));
    g.strokePath(rhythmBeatPath,
                 juce::PathStrokeType(10.0f, juce::PathStrokeType::curved,
                                      juce::PathStrokeType::rounded));

    // Inner Pattern
    g.setColour(juce::Colours::magenta.withAlpha(0.05f));
    g.strokePath(patternRestPath,
                 juce::PathStrokeType(6.0f, juce::PathStrokeType::curved,
                                      juce::PathStrokeType::rounded));

    g.setColour(juce::Colours::magenta.withAlpha(0.2f));
    g.strokePath(patternBeatPath,
                 juce::PathStrokeType(9.0f, juce::PathStrokeType::curved,
                                      juce::PathStrokeType::rounded));
    g.setColour(juce::Colours::magenta.withAlpha(0.6f));
    g.strokePath(patternBeatPath,
                 juce::PathStrokeType(6.0f, juce::PathStrokeType::curved,
                                      juce::PathStrokeType::rounded));

    // --- Draw Playheads (Instantaneous) ---
    // User wants 2 tiers of brightness:
    // 1. Note Played -> Bright (1.0 opacity, with aura)
    // 2. Any Rest -> Dimmer (0.6 opacity)

    // Rhythm Ring Playhead
    drawPlayhead2Tier(g, center, radius, 10.0f, (int)currentRhythm.size(),
                      currentRhythmIdx, currentNotePlayed, juce::Colours::cyan);

    // Pattern Ring Playhead
    drawPlayhead2Tier(g, center, radius - 16.0f, 6.0f,
                      (int)currentPattern.size(), currentPatternIdx,
                      currentNotePlayed, juce::Colours::magenta);
  }

 private:
  void rebuildPaths(juce::Point<float> center, float radius) {
    rhythmBeatPath.clear();
    rhythmRestPath.clear();
    patternBeatPath.clear();
    patternRestPath.clear();

    buildRingPaths(center, radius, currentRhythm, rhythmBeatPath,
                   rhythmRestPath);
    buildRingPaths(center, radius - 16.0f, currentPattern, patternBeatPath,
                   patternRestPath);
  }

  void buildRingPaths(juce::Point<float> center, float radius,
                      const std::vector<bool> &pattern, juce::Path &beatPath,
                      juce::Path &restPath) {
    if (pattern.empty())
      return;

    int numSteps = (int)pattern.size();
    float angleStep = juce::MathConstants<float>::twoPi / (float)numSteps;
    float gap = std::max(0.015f, 0.12f / (float)numSteps);
    float startAngle = -juce::MathConstants<float>::halfPi;

    for (int i = 0; i < numSteps; ++i) {
      float angle = startAngle + (float)i * angleStep;
      if (pattern[(size_t)i])
        beatPath.addCentredArc(center.x, center.y, radius, radius, 0.0f,
                               angle + gap, angle + angleStep - gap, true);
      else
        restPath.addCentredArc(center.x, center.y, radius, radius, 0.0f,
                               angle + gap, angle + angleStep - gap, true);
    }
  }

  void drawPlayhead2Tier(juce::Graphics &g, juce::Point<float> center,
                         float radius, float thickness, int numSteps,
                         int activeIdx, bool isNotePlayed,
                         juce::Colour layerColor) {
    if (numSteps <= 0 || activeIdx < 0)
      return;

    float angleStep = juce::MathConstants<float>::twoPi / (float)numSteps;
    float angle =
        -juce::MathConstants<float>::halfPi + (float)activeIdx * angleStep;
    float gap = std::max(0.01f, 0.08f / (float)numSteps);

    juce::Path p;
    p.addCentredArc(center.x, center.y, radius, radius, 0.0f, angle + gap,
                    angle + angleStep - gap, true);

    if (isNotePlayed) {
      // Note Played: Bright white with colored aura
      g.setColour(layerColor.withAlpha(0.7f));
      g.strokePath(p, juce::PathStrokeType(thickness + 4.0f,
                                           juce::PathStrokeType::curved,
                                           juce::PathStrokeType::rounded));
      g.setColour(juce::Colours::white);
      g.strokePath(p, juce::PathStrokeType(thickness + 1.0f,
                                           juce::PathStrokeType::curved,
                                           juce::PathStrokeType::rounded));
    } else {
      // Any Rest: Dimmer (0.6 opacity)
      g.setColour(layerColor.withAlpha(0.4f));
      g.strokePath(p,
                   juce::PathStrokeType(thickness, juce::PathStrokeType::curved,
                                        juce::PathStrokeType::rounded));
      g.setColour(juce::Colours::white.withAlpha(0.6f));
      g.strokePath(p, juce::PathStrokeType(thickness + 1.0f,
                                           juce::PathStrokeType::curved,
                                           juce::PathStrokeType::rounded));
    }
  }

  std::vector<bool> currentPattern;
  int currentPatternIdx = -1;
  std::vector<bool> currentRhythm;
  int currentRhythmIdx = -1;
  bool currentNotePlayed = false;

  bool pathsDirty = true;
  juce::Path rhythmBeatPath, rhythmRestPath;
  juce::Path patternBeatPath, patternRestPath;
};
