#include "AlgorithmicModulatorNode.h"

#include <algorithm>
#include <cmath>

#include "../EuclideanMath.h"
#include "../GraphCanvas.h"
#include "../LayoutParser.h"
#include "BinaryData.h"

// ──────────────────────────────────────────────────────────────────────────────
// Custom step-value editor for the Custom algorithm
// ──────────────────────────────────────────────────────────────────────────────

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
      auto barRect = juce::Rectangle<float>(b.getX() + (float)i * cellW,
                                            b.getY() + bh - barH,
                                            cellW - 1.0f, barH);

      g.setColour(juce::Colour(0xffaa44ff).withAlpha(0.55f + 0.45f * v));
      g.fillRect(barRect);

      // Step boundary
      g.setColour(juce::Colour(0xff333333));
      g.drawVerticalLine(b.getX() + (int)((float)(i + 1) * cellW), (float)b.getY(),
                         (float)b.getBottom());
    }

    // Horizontal mid-line
    g.setColour(juce::Colour(0xff444444));
    g.drawHorizontalLine(b.getCentreY(), (float)b.getX(), (float)b.getRight());
  }

  void mouseDown(const juce::MouseEvent &e) override { handleDrag(e); }
  void mouseDrag(const juce::MouseEvent &e) override { handleDrag(e); }

 private:
  void handleDrag(const juce::MouseEvent &e) {
    if (modNode.algorithm != AlgorithmicModulatorNode::Custom) return;

    auto b = getLocalBounds();
    int len = std::max(1, (int)modNode.customValues.size());
    float cellW = (float)b.getWidth() / (float)len;
    int col = (int)((float)(e.x - b.getX()) / cellW);

    if (col < 0 || col >= len) return;

    float v = 1.0f - std::clamp(
                         (float)(e.y - b.getY()) / (float)b.getHeight(),
                         0.0f, 1.0f);

    auto *canvas = findParentComponentOfClass<GraphCanvas>();
    if (canvas != nullptr) {
      canvas->performMutation([this, col, v]() {
        modNode.customValues[(size_t)col] = v;
        if (modNode.onNodeDirtied) modNode.onNodeDirtied();
      });
    } else {
      modNode.customValues[(size_t)col] = v;
      if (modNode.onNodeDirtied) modNode.onNodeDirtied();
    }
    repaint();
  }

  void timerCallback() override { repaint(); }

  AlgorithmicModulatorNode &modNode;
};

// ──────────────────────────────────────────────────────────────────────────────
// AlgorithmicModulatorNode implementation
// ──────────────────────────────────────────────────────────────────────────────

AlgorithmicModulatorNode::AlgorithmicModulatorNode() {
  addMacroParam(&macroAlgorithm);
  addMacroParam(&macroCCNumber);
  addMacroParam(&macroLength);
  addMacroParam(&macroRangeMin);
  addMacroParam(&macroRangeMax);
  addMacroParam(&macroSteps);
  addMacroParam(&macroBeats);
  addMacroParam(&macroOffset);
  ensureCustomValues();
}

void AlgorithmicModulatorNode::ensureCustomValues() {
  customValues.resize((size_t)std::max(1, length), 0.5f);
}

NodeLayout AlgorithmicModulatorNode::getLayout() const {
  auto layout =
      LayoutParser::parseFromJSON(BinaryData::AlgorithmicModulatorNode_json,
                                  BinaryData::AlgorithmicModulatorNode_jsonSize);

  for (auto &el : layout.elements) {
    if (el.label == "algorithm") {
      el.valueRef = const_cast<int *>(&algorithm);
      el.macroParamRef = const_cast<MacroParam *>(&macroAlgorithm);
    } else if (el.label == "ccNumber") {
      el.valueRef = const_cast<int *>(&ccNumber);
      el.macroParamRef = const_cast<MacroParam *>(&macroCCNumber);
    } else if (el.label == "length") {
      el.valueRef = const_cast<int *>(&length);
      el.macroParamRef = const_cast<MacroParam *>(&macroLength);
    } else if (el.label == "rangeMin") {
      el.floatValueRef = const_cast<float *>(&rangeMin);
      el.macroParamRef = const_cast<MacroParam *>(&macroRangeMin);
    } else if (el.label == "rangeMax") {
      el.floatValueRef = const_cast<float *>(&rangeMax);
      el.macroParamRef = const_cast<MacroParam *>(&macroRangeMax);
    } else if (el.label == "steps") {
      el.valueRef = const_cast<int *>(&steps);
      el.macroParamRef = const_cast<MacroParam *>(&macroSteps);
    } else if (el.label == "beats") {
      el.valueRef = const_cast<int *>(&beats);
      el.macroParamRef = const_cast<MacroParam *>(&macroBeats);
    } else if (el.label == "offset") {
      el.valueRef = const_cast<int *>(&offset);
      el.macroParamRef = const_cast<MacroParam *>(&macroOffset);
    }
  }

  for (auto &el : layout.unfoldedElements) {
    if (el.label == "algorithm") {
      el.valueRef = const_cast<int *>(&algorithm);
      el.macroParamRef = const_cast<MacroParam *>(&macroAlgorithm);
    } else if (el.label == "ccNumber") {
      el.valueRef = const_cast<int *>(&ccNumber);
      el.macroParamRef = const_cast<MacroParam *>(&macroCCNumber);
    } else if (el.label == "length") {
      el.valueRef = const_cast<int *>(&length);
      el.macroParamRef = const_cast<MacroParam *>(&macroLength);
    } else if (el.label == "rangeMin") {
      el.floatValueRef = const_cast<float *>(&rangeMin);
      el.macroParamRef = const_cast<MacroParam *>(&macroRangeMin);
    } else if (el.label == "rangeMax") {
      el.floatValueRef = const_cast<float *>(&rangeMax);
      el.macroParamRef = const_cast<MacroParam *>(&macroRangeMax);
    } else if (el.label == "steps") {
      el.valueRef = const_cast<int *>(&steps);
      el.macroParamRef = const_cast<MacroParam *>(&macroSteps);
    } else if (el.label == "beats") {
      el.valueRef = const_cast<int *>(&beats);
      el.macroParamRef = const_cast<MacroParam *>(&macroBeats);
    } else if (el.label == "offset") {
      el.valueRef = const_cast<int *>(&offset);
      el.macroParamRef = const_cast<MacroParam *>(&macroOffset);
    }
  }

  return layout;
}

std::unique_ptr<juce::Component>
AlgorithmicModulatorNode::createCustomComponent(
    const juce::String &name, juce::AudioProcessorValueTreeState *apvts) {
  if (name == "ModulatorStepEditor" && apvts != nullptr) {
    return std::make_unique<ModulatorStepEditor>(*this, *apvts);
  }
  return nullptr;
}

void AlgorithmicModulatorNode::process() {
  int actualAlgo   = resolveMacroInt(macroAlgorithm, algorithm, 0, (int)Custom);
  int actualLength = resolveMacroInt(macroLength, length, 1, 64);
  int actualCC = resolveMacroInt(macroCCNumber, ccNumber, 0, 127);
  float actualMin = resolveMacroFloat(macroRangeMin, rangeMin, 0.0f, 1.0f);
  float actualMax = resolveMacroFloat(macroRangeMax, rangeMax, 0.0f, 1.0f);

  EventSequence outSeq;

  if (actualAlgo == EuclideanGates) {
    int actualSteps = resolveMacroInt(macroSteps, steps, 1, 64);
    int actualBeats = resolveMacroInt(macroBeats, beats, 1, actualSteps);
    int actualOffset = resolveMacroInt(macroOffset, offset, 0, 63);

    auto pattern =
        EuclideanMath::generatePattern(actualSteps, actualBeats, actualOffset);
    outSeq.reserve(pattern.size());
    for (bool gate : pattern) {
      if (gate) {
        outSeq.push_back({CCEvent{actualCC, actualMax, 1}});
      } else {
        outSeq.emplace_back();
      }
    }
  } else {
    outSeq.reserve((size_t)actualLength);

    // Seed from nodeId for stable, deterministic random sequences
    uint32_t seed = 0;
    for (juce::juce_wchar c : nodeId.toString()) {
      seed = seed * 31u + static_cast<uint32_t>(c);
    }
    juce::Random rng(static_cast<juce::int64>(seed));

    float prevValue = 0.5f;

    for (int i = 0; i < actualLength; ++i) {
      float normalValue = 0.0f;
      const float pi = 3.14159265f;

      switch (actualAlgo) {
        case Sine:
          normalValue =
              0.5f * (1.0f + std::sin(2.0f * pi * (float)i / (float)actualLength));
          break;
        case RampUp:
          normalValue =
              (actualLength > 1) ? (float)i / (float)(actualLength - 1) : 0.0f;
          break;
        case RampDown:
          normalValue = (actualLength > 1)
                            ? 1.0f - (float)i / (float)(actualLength - 1)
                            : 1.0f;
          break;
        case Triangle: {
          float t = (float)i / (float)actualLength;
          normalValue = (t < 0.5f) ? t * 2.0f : (1.0f - t) * 2.0f;
          break;
        }
        case RandomHold:
          normalValue = rng.nextFloat();
          break;
        case RandomWalk: {
          float delta = (rng.nextFloat() - 0.5f) * 0.25f;
          prevValue = std::clamp(prevValue + delta, 0.0f, 1.0f);
          normalValue = prevValue;
          break;
        }
        case Custom:
          normalValue = (i < (int)customValues.size())
                            ? std::clamp(customValues[(size_t)i], 0.0f, 1.0f)
                            : 0.5f;
          break;
        default:
          normalValue = 0.0f;
          break;
      }

      float value = actualMin + (actualMax - actualMin) * normalValue;
      outSeq.push_back({CCEvent{actualCC, value, 1}});
    }
  }

  outputSequences[0] = outSeq;

  auto connIt = connections.find(0);
  if (connIt != connections.end()) {
    for (const auto &conn : connIt->second) {
      conn.targetNode->setInputSequence(conn.targetInputPort, outputSequences[0]);
    }
  }
}

void AlgorithmicModulatorNode::saveNodeState(juce::XmlElement *xml) {
  if (!xml) return;

  xml->setAttribute("algorithm", algorithm);
  xml->setAttribute("ccNumber", ccNumber);
  xml->setAttribute("length", length);
  xml->setAttribute("rangeMin", rangeMin);
  xml->setAttribute("rangeMax", rangeMax);
  xml->setAttribute("steps", steps);
  xml->setAttribute("beats", beats);
  xml->setAttribute("offset", offset);

  // Serialise customValues as comma-separated floats
  juce::String cvStr;
  for (size_t i = 0; i < customValues.size(); ++i) {
    if (i > 0) cvStr += ",";
    cvStr += juce::String(customValues[i], 4);
  }
  xml->setAttribute("customValues", cvStr);

  saveMacroBindings(xml);
}

void AlgorithmicModulatorNode::loadNodeState(juce::XmlElement *xml) {
  if (!xml) return;

  algorithm = xml->getIntAttribute("algorithm", 0);
  ccNumber = xml->getIntAttribute("ccNumber", 1);
  length = xml->getIntAttribute("length", 8);
  rangeMin = (float)xml->getDoubleAttribute("rangeMin", 0.0);
  rangeMax = (float)xml->getDoubleAttribute("rangeMax", 1.0);
  steps = xml->getIntAttribute("steps", 8);
  beats = xml->getIntAttribute("beats", 4);
  offset = xml->getIntAttribute("offset", 0);

  customValues.clear();
  juce::String cvStr = xml->getStringAttribute("customValues");
  if (cvStr.isNotEmpty()) {
    juce::StringArray tokens;
    tokens.addTokens(cvStr, ",", "");
    for (const auto &t : tokens) {
      customValues.push_back(std::clamp(t.getFloatValue(), 0.0f, 1.0f));
    }
  }
  ensureCustomValues();

  loadMacroBindings(xml);
}
