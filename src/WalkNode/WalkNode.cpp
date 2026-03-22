#include "WalkNode.h"
#include "../MacroMappingMenu.h"
#include <algorithm>
#include <random>

// --- WalkNode Impl

WalkNode::WalkNode(std::array<std::atomic<float> *, 32> &inMacros)
    : macros(inMacros) {}

NodeLayout WalkNode::getLayout() const {
  NodeLayout layout;
  layout.gridWidth = 2;
  layout.gridHeight = 2;

  UIElement lenLabel;
  lenLabel.type = UIElementType::Label;
  lenLabel.label = "Length";
  lenLabel.gridBounds = {0, 0, 3, 1};
  layout.elements.push_back(lenLabel);

  UIElement lenSlider;
  lenSlider.type = UIElementType::RotarySlider;
  lenSlider.minValue = 1;
  lenSlider.maxValue = 64;
  lenSlider.valueRef = const_cast<int *>(&walkLength);
  lenSlider.macroIndexRef = const_cast<int *>(&macroWalkLength);
  lenSlider.gridBounds = {0, 1, 3, 4};
  layout.elements.push_back(lenSlider);

  UIElement skewLabel;
  skewLabel.type = UIElementType::Label;
  skewLabel.label = "Skew (%)";
  skewLabel.gridBounds = {3, 0, 3, 1};
  layout.elements.push_back(skewLabel);

  UIElement skewSlider;
  skewSlider.type = UIElementType::RotarySlider;
  skewSlider.minValue = -100;
  skewSlider.maxValue = 100;
  skewSlider.valueRef = const_cast<int *>(&walkSkewInt);
  skewSlider.macroIndexRef = const_cast<int *>(&macroWalkSkew);
  skewSlider.gridBounds = {3, 1, 3, 4};
  layout.elements.push_back(skewSlider);

  return layout;
}

void WalkNode::saveNodeState(juce::XmlElement *xml) {
  if (xml != nullptr) {
    xml->setAttribute("walkLength", walkLength);
    xml->setAttribute("macroWalkLength", macroWalkLength);
    xml->setAttribute("walkSkewInt", walkSkewInt);
    xml->setAttribute("macroWalkSkew", macroWalkSkew);
  }
}

void WalkNode::loadNodeState(juce::XmlElement *xml) {
  if (xml != nullptr) {
    walkLength = xml->getIntAttribute("walkLength", 16);
    macroWalkLength = xml->getIntAttribute("macroWalkLength", -1);
    macroWalkSkew = xml->getIntAttribute("macroWalkSkew", -1);
    if (xml->hasAttribute("walkSkewInt")) {
      walkSkewInt = xml->getIntAttribute("walkSkewInt", 0);
    } else {
      walkSkewInt = (int)(xml->getDoubleAttribute("walkSkew", 0.0) * 100.0);
    }
  }
}

void WalkNode::process() {
  int actualLength =
      macroWalkLength != -1 && macros[(size_t)macroWalkLength] != nullptr
          ? 1 + (int)std::round(macros[(size_t)macroWalkLength]->load() * 63.0f)
          : walkLength;

  float actualSkew =
      macroWalkSkew != -1 && macros[(size_t)macroWalkSkew] != nullptr
          ? (macros[(size_t)macroWalkSkew]->load() * 2.0f) - 1.0f
          : (walkSkewInt / 100.0f);

  auto it = inputSequences.find(0);
  if (it == inputSequences.end() || it->second.empty() || actualLength <= 0) {
    outputSequences[0] = NoteSequence();
  } else {
    NoteSequence steps = it->second;

    auto getMeanValue = [](const std::vector<HeldNote> &chord) {
      if (chord.empty())
        return 0.0f;
      float sum = 0.0f;
      for (const auto &n : chord)
        sum += n.noteNumber;
      return sum / (float)chord.size();
    };

    std::stable_sort(steps.begin(), steps.end(),
                     [&getMeanValue](const std::vector<HeldNote> &a,
                                     const std::vector<HeldNote> &b) {
                       return getMeanValue(a) < getMeanValue(b);
                     });

    // Generate a seed based on the sorted steps and length parameter so it's
    // stable
    size_t seed = steps.size() + (size_t)actualLength;
    for (const auto &step : steps) {
      for (const auto &n : step) {
        seed ^= (size_t)n.noteNumber + 0x9e3779b9 + (seed << 6) + (seed >> 2);
      }
    }

    std::mt19937 gen((unsigned int)seed);
    std::uniform_real_distribution<float> dis(0.0f, 1.0f);

    actualSkew = std::max(-1.0f, std::min(1.0f, actualSkew));
    float pv = std::max(0.0f, (1.0f / 3.0f) * (1.0f - actualSkew));
    float pn = std::max(0.0f, (1.0f / 3.0f) * (1.0f + actualSkew));
    float pc = std::max(0.0f, 1.0f - pv - pn);

    float total = pv + pc + pn;
    if (total > 0.0f) {
      pv /= total;
      pc /= total;
      pn /= total;
    } else {
      pv = pc = pn = 1.0f / 3.0f;
    }

    NoteSequence generatedSeq;

    if (steps.size() > 0) {
      int currentIdx = (int)steps.size() / 2;

      for (int i = 0; i < actualLength; ++i) {
        generatedSeq.push_back(steps[(size_t)currentIdx]);

        float r = dis(gen);
        if (r < pv) {
          currentIdx = (currentIdx - 1 + (int)steps.size()) % (int)steps.size();
        } else if (r >= pv + pc) {
          currentIdx = (currentIdx + 1) % (int)steps.size();
        }
      }
    }

    outputSequences[0] = generatedSeq;
  }

  auto connIt = connections.find(0);
  if (connIt != connections.end()) {
    for (const auto &connection : connIt->second) {
      connection.targetNode->setInputSequence(connection.targetInputPort,
                                              outputSequences[0]);
    }
  }
}
