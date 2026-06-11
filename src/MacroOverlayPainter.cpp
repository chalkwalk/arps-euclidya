#include "MacroOverlayPainter.h"

#include "MacroColours.h"

namespace MacroOverlay {

void paint(juce::Graphics &g, GraphNode *node,
           const std::vector<SliderMacroInfo> &sliderInfos,
           const std::vector<ButtonMacroInfo> &buttonInfos,
           const std::vector<ComboMacroInfo> &comboInfos, int selectedMacro,
           int highlightedMacro) {
  // Always draw intensity arcs for existing bindings, regardless of selection
  for (const auto &info : sliderInfos) {
    if (info.macroParamRef == nullptr || info.macroParamRef->bindings.empty()) {
      continue;
    }

    auto sliderBounds = info.slider->getBounds().toFloat().reduced(2.0f);
    float radius =
        (juce::jmin(sliderBounds.getWidth(), sliderBounds.getHeight()) / 2.0f) -
        2.0f;
    if (radius <= 0.0f) {
      continue;
    }

    float cx = sliderBounds.getCentreX();
    float cy = sliderBounds.getCentreY();
    auto rp = info.slider->getRotaryParameters();
    float sweep = rp.endAngleRadians - rp.startAngleRadians;
    float trackWidth = radius * 0.4f;
    float arcStroke = 2.5f;
    float arcGap = arcStroke + 1.0f;
    float firstArcRadius = radius + (trackWidth * 0.5f) + 2.0f;

    int ringIndex = 0;
    for (const auto &binding : info.macroParamRef->bindings) {
      float absIntensity = std::abs(binding.intensity);
      if (absIntensity < 0.001f) {
        continue;
      }

      float arcRadius =
          firstArcRadius + (static_cast<float>(ringIndex) * arcGap);
      ++ringIndex;

      auto colour = getMacroColour(binding.macroIndex);
      float arcAngle = (absIntensity * sweep) * 0.5f;

      bool isBipolar =
          (node->macroBipolarMask.load(std::memory_order_relaxed) >>
           (unsigned)binding.macroIndex) &
          1u;

      float arcStart = 0.0f;
      float arcEnd = 0.0f;
      if (isBipolar) {
        float centre = rp.startAngleRadians + (sweep * 0.5f);
        arcStart = centre - arcAngle;
        arcEnd = centre + arcAngle;
      } else if (binding.intensity >= 0.0f) {
        arcStart = rp.startAngleRadians;
        arcEnd = rp.startAngleRadians + arcAngle;
      } else {
        arcEnd = rp.endAngleRadians;
        arcStart = rp.endAngleRadians - arcAngle;
      }

      juce::Path arc;
      arc.addCentredArc(cx, cy, arcRadius, arcRadius, 0.0f, arcStart, arcEnd,
                        true);
      g.setColour(colour.withAlpha(0.7f));
      g.strokePath(arc,
                   juce::PathStrokeType(arcStroke, juce::PathStrokeType::curved,
                                        juce::PathStrokeType::rounded));
    }

    // Effective value indicator: arc from set value to effective value + ring
    {
      auto setVal = (float)info.slider->getValue();
      auto minVal = (float)info.slider->getMinimum();
      auto maxVal = (float)info.slider->getMaximum();
      float range = maxVal - minVal;
      float effectiveVal =
          node->resolveMacroFloat(*info.macroParamRef, setVal, minVal, maxVal);

      float setPos = range > 0.0f
                         ? juce::jlimit(0.0f, 1.0f, (setVal - minVal) / range)
                         : 0.0f;
      float effectivePos =
          range > 0.0f
              ? juce::jlimit(0.0f, 1.0f, (effectiveVal - minVal) / range)
              : 0.0f;

      float setAngle = rp.startAngleRadians + (setPos * sweep);
      float effectiveAngle = rp.startAngleRadians + (effectivePos * sweep);

      if (std::abs(effectiveAngle - setAngle) > 0.01f) {
        float arcFrom = juce::jmin(setAngle, effectiveAngle);
        float arcTo = juce::jmax(setAngle, effectiveAngle);
        juce::Path bridgeArc;
        bridgeArc.addCentredArc(cx, cy, radius, radius, 0.0f, arcFrom, arcTo,
                                true);
        g.setColour(juce::Colours::white.withAlpha(0.5f));
        g.strokePath(bridgeArc,
                     juce::PathStrokeType(trackWidth * 0.35f,
                                          juce::PathStrokeType::curved,
                                          juce::PathStrokeType::rounded));
      }

      float ex = cx + (radius * std::cos(effectiveAngle -
                                         juce::MathConstants<float>::halfPi));
      float ey = cy + (radius * std::sin(effectiveAngle -
                                         juce::MathConstants<float>::halfPi));
      g.setColour(juce::Colours::white.withAlpha(0.9f));
      g.drawEllipse(ex - 4.0f, ey - 4.0f, 8.0f, 8.0f, 1.5f);
    }
  }

  // Button binding indicators
  for (const auto &info : buttonInfos) {
    if (info.macroParamRef == nullptr || info.macroParamRef->bindings.empty()) {
      continue;
    }

    auto bounds = info.button->getBoundsInParent().toFloat();
    auto primaryColour =
        getMacroColour(info.macroParamRef->bindings[0].macroIndex);

    {
      int borderIndex = 0;
      for (const auto &binding : info.macroParamRef->bindings) {
        auto bindColour = getMacroColour(binding.macroIndex);
        float inset = 0.5f + (static_cast<float>(borderIndex) * 2.5f);
        g.setColour(bindColour.withAlpha(0.75f));
        g.drawRoundedRectangle(bounds.reduced(inset), 3.0f, 1.5f);
        ++borderIndex;
      }
    }

    {
      float totalIntensity = 0.0f;
      for (const auto &b : info.macroParamRef->bindings) {
        totalIntensity += b.intensity;
      }
      float absI = juce::jlimit(0.0f, 2.0f, std::abs(totalIntensity));
      if (absI > 0.01f) {
        float pad = 4.0f;
        float maxW = bounds.getWidth() - (pad * 2.0f);
        float barW = (absI / 2.0f) * maxW;
        float barX = bounds.getX() + pad;
        float barY = bounds.getBottom() - 3.5f;
        auto barColour = (totalIntensity >= 0.0f) ? primaryColour
                                                  : primaryColour.darker(0.4f);
        g.setColour(barColour.withAlpha(0.85f));
        g.fillRoundedRectangle(barX, barY, barW, 2.0f, 1.0f);
      }
    }

    if (info.valueRef != nullptr) {
      int localVal = *info.valueRef;
      int effectiveVal =
          node->resolveMacroInt(*info.macroParamRef, localVal, 0, 1);
      if (effectiveVal != localVal) {
        float dotR = 3.5f;
        float dotX = bounds.getRight() - (dotR * 2.0f) - 3.0f;
        float dotY = bounds.getCentreY() - dotR;
        if (effectiveVal == 1) {
          g.setColour(primaryColour.withAlpha(0.95f));
          g.fillEllipse(dotX, dotY, dotR * 2.0f, dotR * 2.0f);
        } else {
          g.setColour(primaryColour.withAlpha(0.95f));
          g.drawEllipse(dotX, dotY, dotR * 2.0f, dotR * 2.0f, 1.5f);
        }
      }
    }
  }

  // ComboBox binding indicators
  for (const auto &info : comboInfos) {
    if (info.macroParamRef == nullptr || info.macroParamRef->bindings.empty()) {
      continue;
    }

    auto bounds = info.combo->getBoundsInParent().toFloat();

    int borderIndex = 0;
    for (const auto &binding : info.macroParamRef->bindings) {
      auto bindColour = getMacroColour(binding.macroIndex);
      float inset = 0.5f + (static_cast<float>(borderIndex) * 2.5f);
      g.setColour(bindColour.withAlpha(0.75f));
      g.drawRoundedRectangle(bounds.reduced(inset), 3.0f, 1.5f);
      ++borderIndex;
    }
  }

  // Palette hover: bright white ring around controls bound to the hovered macro
  if (highlightedMacro != -1) {
    for (const auto &info : sliderInfos) {
      if (info.macroParamRef == nullptr)
        continue;
      bool bound = std::any_of(info.macroParamRef->bindings.begin(),
                               info.macroParamRef->bindings.end(),
                               [highlightedMacro](const MacroBinding &b) {
                                 return b.macroIndex == highlightedMacro;
                               });
      if (bound) {
        auto bounds = info.slider->getBounds().toFloat();
        float r =
            (juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f) + 2.0f;
        auto centre = bounds.getCentre();
        g.setColour(juce::Colours::white);
        g.drawEllipse(centre.x - r, centre.y - r, r * 2.0f, r * 2.0f, 2.0f);
      }
    }
    for (const auto &info : buttonInfos) {
      if (info.macroParamRef == nullptr)
        continue;
      bool bound = std::any_of(info.macroParamRef->bindings.begin(),
                               info.macroParamRef->bindings.end(),
                               [highlightedMacro](const MacroBinding &b) {
                                 return b.macroIndex == highlightedMacro;
                               });
      if (bound) {
        auto bounds = info.button->getBoundsInParent().toFloat();
        g.setColour(juce::Colours::white);
        g.drawRoundedRectangle(bounds.expanded(2.0f), 3.0f, 2.0f);
      }
    }
    for (const auto &info : comboInfos) {
      if (info.macroParamRef == nullptr)
        continue;
      bool bound = std::any_of(info.macroParamRef->bindings.begin(),
                               info.macroParamRef->bindings.end(),
                               [highlightedMacro](const MacroBinding &b) {
                                 return b.macroIndex == highlightedMacro;
                               });
      if (bound) {
        auto bounds = info.combo->getBoundsInParent().toFloat();
        g.setColour(juce::Colours::white);
        g.drawRoundedRectangle(bounds.expanded(2.0f), 3.0f, 2.0f);
      }
    }
  }

  // Draw faint "bindable" rings when a macro is selected
  if (selectedMacro == -1) {
    return;
  }

  int macroIdx = selectedMacro;
  auto colour = getMacroColour(macroIdx);

  for (const auto &info : sliderInfos) {
    bool alreadyBound = false;
    if (info.macroParamRef != nullptr) {
      for (const auto &b : info.macroParamRef->bindings) {
        if (b.macroIndex == macroIdx) {
          alreadyBound = true;
          break;
        }
      }
    }

    if (!alreadyBound) {
      auto bounds = info.slider->getBounds().toFloat();
      float r =
          (juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f) - 1.0f;
      if (r > 0.0f) {
        auto centre = bounds.getCentre();
        g.setColour(colour.withAlpha(0.30f));
        g.drawEllipse(centre.x - r, centre.y - r, r * 2.0f, r * 2.0f, 2.0f);
      }
    }
  }

  for (const auto &info : buttonInfos) {
    bool alreadyBound = false;
    for (const auto &b : info.macroParamRef->bindings) {
      if (b.macroIndex == macroIdx) {
        alreadyBound = true;
        break;
      }
    }
    if (!alreadyBound) {
      auto bounds = info.button->getBoundsInParent().toFloat().reduced(0.5f);
      g.setColour(colour.withAlpha(0.30f));
      g.drawRoundedRectangle(bounds, 3.0f, 2.0f);
    }
  }

  for (const auto &info : comboInfos) {
    bool alreadyBound = false;
    for (const auto &b : info.macroParamRef->bindings) {
      if (b.macroIndex == macroIdx) {
        alreadyBound = true;
        break;
      }
    }
    if (!alreadyBound) {
      auto bounds = info.combo->getBoundsInParent().toFloat().reduced(0.5f);
      g.setColour(colour.withAlpha(0.30f));
      g.drawRoundedRectangle(bounds, 3.0f, 2.0f);
    }
  }
}

}  // namespace MacroOverlay
