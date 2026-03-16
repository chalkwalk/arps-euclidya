#include "WalkNode.h"
#include "MacroMappingMenu.h"
#include <algorithm>
#include <random>

class WalkNodeEditor : public juce::Component {
public:
  WalkNodeEditor(WalkNode &node, juce::AudioProcessorValueTreeState &apvts)
      : walkNode(node) {

    auto setupIntSlider = [this, &apvts](
                              CustomMacroSlider &slider, juce::Label &label,
                              int &nodeValueRef, int &nodeMacroRef,
                              std::unique_ptr<MacroAttachment> &attachment,
                              const juce::String &labelText, int min, int max) {
      slider.setRange(min, max, 1);
      slider.setValue(nodeValueRef, juce::dontSendNotification);
      slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
      slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
      addAndMakeVisible(slider);

      label.setText(labelText, juce::dontSendNotification);
      label.setJustificationType(juce::Justification::centred);
      addAndMakeVisible(label);

      slider.onValueChange = [this, &slider, &nodeValueRef]() {
        nodeValueRef = (int)slider.getValue();
        if (walkNode.onNodeDirtied)
          walkNode.onNodeDirtied();
      };

      auto updateSliderVisibility = [&slider](int macro) {
        if (macro == -1) {
          slider.removeColour(juce::Slider::rotarySliderFillColourId);
          slider.removeColour(juce::Slider::rotarySliderOutlineColourId);
        } else {
          slider.setColour(juce::Slider::rotarySliderFillColourId,
                           juce::Colours::orange);
          slider.setColour(juce::Slider::rotarySliderOutlineColourId,
                           juce::Colours::orange.withAlpha(0.3f));
        }
      };

      updateSliderVisibility(nodeMacroRef);

      slider.onRightClick = [this, &slider, &nodeMacroRef, &attachment, &apvts,
                             updateSliderVisibility]() {
        MacroMappingMenu::showMenu(
            &slider, nodeMacroRef,
            [this, &nodeMacroRef, &attachment, &apvts, &slider,
             updateSliderVisibility](int macroIndex) {
              nodeMacroRef = macroIndex;
              if (macroIndex == -1) {
                attachment.reset();
                slider.setTooltip("");
              } else {
                attachment = std::make_unique<MacroAttachment>(
                    apvts, "macro_" + juce::String(macroIndex + 1), slider);
                slider.setTooltip("Mapped to Macro " +
                                  juce::String(macroIndex + 1));
              }
              updateSliderVisibility(macroIndex);
              if (walkNode.onMappingChanged)
                walkNode.onMappingChanged();
            });
      };

      // INIT
      if (nodeMacroRef != -1) {
        juce::String paramID = "macro_" + juce::String(nodeMacroRef + 1);
        attachment = std::make_unique<MacroAttachment>(apvts, paramID, slider);
      }
    };

    auto setupFloatSlider = [this, &apvts](
                                CustomMacroSlider &slider, juce::Label &label,
                                float &nodeValueRef, int &nodeMacroRef,
                                std::unique_ptr<MacroAttachment> &attachment,
                                const juce::String &labelText, float min,
                                float max, float step = 1.0f) {
      slider.setRange(min, max, step);
      slider.setValue(nodeValueRef, juce::dontSendNotification);
      slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
      slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
      addAndMakeVisible(slider);

      label.setText(labelText, juce::dontSendNotification);
      label.setJustificationType(juce::Justification::centred);
      addAndMakeVisible(label);

      slider.onValueChange = [this, &slider, &nodeValueRef]() {
        nodeValueRef = (float)slider.getValue();
        if (walkNode.onNodeDirtied)
          walkNode.onNodeDirtied();
      };

      auto updateSliderVisibility = [&slider](int macro) {
        if (macro == -1) {
          slider.removeColour(juce::Slider::rotarySliderFillColourId);
          slider.removeColour(juce::Slider::rotarySliderOutlineColourId);
        } else {
          slider.setColour(juce::Slider::rotarySliderFillColourId,
                           juce::Colours::orange);
          slider.setColour(juce::Slider::rotarySliderOutlineColourId,
                           juce::Colours::orange.withAlpha(0.3f));
        }
      };

      updateSliderVisibility(nodeMacroRef);

      slider.onRightClick = [this, &slider, &nodeMacroRef, &attachment, &apvts,
                             updateSliderVisibility]() {
        MacroMappingMenu::showMenu(
            &slider, nodeMacroRef,
            [this, &nodeMacroRef, &attachment, &apvts, &slider,
             updateSliderVisibility](int macroIndex) {
              nodeMacroRef = macroIndex;
              if (macroIndex == -1) {
                attachment.reset();
                slider.setTooltip("");
              } else {
                attachment = std::make_unique<MacroAttachment>(
                    apvts, "macro_" + juce::String(macroIndex + 1), slider);
                slider.setTooltip("Mapped to Macro " +
                                  juce::String(macroIndex + 1));
              }
              updateSliderVisibility(macroIndex);
              if (walkNode.onMappingChanged)
                walkNode.onMappingChanged();
            });
      };

      // INIT
      if (nodeMacroRef != -1) {
        juce::String paramID = "macro_" + juce::String(nodeMacroRef + 1);
        attachment = std::make_unique<MacroAttachment>(apvts, paramID, slider);
      }
    };

    setupIntSlider(lengthSlider, lengthLabel, walkNode.walkLength,
                   walkNode.macroWalkLength, lengthAttachment, "Walk Length", 1,
                   64);

    setupFloatSlider(skewSlider, skewLabel, walkNode.walkSkew,
                     walkNode.macroWalkSkew, skewAttachment, "Walk Skew", -1.0f,
                     1.0f, 0.01f);

    setSize(400, 150);
  }

  void resized() override {
    auto bounds = getLocalBounds().reduced(10);
    auto topHalf = bounds.removeFromTop(bounds.getHeight() / 2);

    // Length slider in top half
    lengthLabel.setBounds(topHalf.removeFromTop(20));
    int size1 = std::min(topHalf.getWidth(), topHalf.getHeight());
    lengthSlider.setBounds(topHalf.withSizeKeepingCentre(size1, size1));

    // Skew slider in bottom half
    skewLabel.setBounds(bounds.removeFromTop(20));
    int size2 = std::min(bounds.getWidth(), bounds.getHeight());
    skewSlider.setBounds(bounds.withSizeKeepingCentre(size2, size2));
  }

private:
  WalkNode &walkNode;
  CustomMacroSlider lengthSlider;
  juce::Label lengthLabel;
  std::unique_ptr<MacroAttachment> lengthAttachment;

  CustomMacroSlider skewSlider;
  juce::Label skewLabel;
  std::unique_ptr<MacroAttachment> skewAttachment;
};

// --- WalkNode Impl

WalkNode::WalkNode(std::array<std::atomic<float> *, 32> &inMacros)
    : macros(inMacros) {}

std::unique_ptr<juce::Component>
WalkNode::createEditorComponent(juce::AudioProcessorValueTreeState &apvts) {
  return std::make_unique<WalkNodeEditor>(*this, apvts);
}

void WalkNode::saveNodeState(juce::XmlElement *xml) {
  if (xml != nullptr) {
    xml->setAttribute("walkLength", walkLength);
    xml->setAttribute("macroWalkLength", macroWalkLength);
    xml->setAttribute("walkSkew", walkSkew);
    xml->setAttribute("macroWalkSkew", macroWalkSkew);
  }
}

void WalkNode::loadNodeState(juce::XmlElement *xml) {
  if (xml != nullptr) {
    walkLength = xml->getIntAttribute("walkLength", 16);
    macroWalkLength = xml->getIntAttribute("macroWalkLength", -1);
    walkSkew = xml->getDoubleAttribute("walkSkew", 0.0);
    macroWalkSkew = xml->getIntAttribute("macroWalkSkew", -1);
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
          : walkSkew;

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
