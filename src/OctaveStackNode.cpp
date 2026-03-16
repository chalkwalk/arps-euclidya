#include "OctaveStackNode.h"
#include "MacroMappingMenu.h"
#include <algorithm>
#include <set>

class OctaveStackNodeEditor : public juce::Component {
public:
  OctaveStackNodeEditor(OctaveStackNode &node,
                        juce::AudioProcessorValueTreeState &apvts)
      : octaveNode(node) {

    auto setupSlider = [this, &apvts](
                           CustomMacroSlider &slider, juce::Label &label,
                           int &nodeValueRef, int &nodeMacroRef,
                           std::unique_ptr<MacroAttachment> &attachment,
                           const juce::String &labelText, int min, int max) {
      slider.setRange(min, max, 1);
      slider.setValue(nodeValueRef, juce::dontSendNotification);
      slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
      slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
      addAndMakeVisible(slider);

      label.setText(labelText, juce::dontSendNotification);
      label.setJustificationType(juce::Justification::centred);
      addAndMakeVisible(label);

      slider.onValueChange = [this, &slider, &nodeValueRef]() {
        nodeValueRef = (int)slider.getValue();
        if (octaveNode.onNodeDirtied)
          octaveNode.onNodeDirtied();
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
              if (octaveNode.onMappingChanged)
                octaveNode.onMappingChanged();
            });
      };

      // INIT
      if (nodeMacroRef != -1) {
        juce::String paramID = "macro_" + juce::String(nodeMacroRef + 1);
        attachment = std::make_unique<MacroAttachment>(apvts, paramID, slider);
      }
    };

    setupSlider(octavesSlider, octavesLabel, octaveNode.octaves,
                octaveNode.macroOctaves, octavesAttachment, "OCT STACK", 1, 4);

    uniqueToggle.setButtonText("Unique Only");
    uniqueToggle.setToggleState(octaveNode.uniqueOnly,
                                juce::dontSendNotification);
    uniqueToggle.onClick = [this]() {
      octaveNode.uniqueOnly = uniqueToggle.getToggleState();
      if (octaveNode.onNodeDirtied)
        octaveNode.onNodeDirtied();
    };
    addAndMakeVisible(uniqueToggle);

    setSize(100, 100);
  }

  void resized() override {
    auto b = getLocalBounds().reduced(2);
    octavesLabel.setBounds(b.removeFromTop(16));
    uniqueToggle.setBounds(b.removeFromBottom(18));
    octavesSlider.setBounds(b);
  }

private:
  OctaveStackNode &octaveNode;
  CustomMacroSlider octavesSlider;
  juce::Label octavesLabel;
  std::unique_ptr<MacroAttachment> octavesAttachment;
  juce::ToggleButton uniqueToggle;
};

// --- OctaveStackNode Impl

OctaveStackNode::OctaveStackNode(std::array<std::atomic<float> *, 32> &inMacros)
    : macros(inMacros) {}

std::unique_ptr<juce::Component> OctaveStackNode::createEditorComponent(
    juce::AudioProcessorValueTreeState &apvts) {
  return std::make_unique<OctaveStackNodeEditor>(*this, apvts);
}

void OctaveStackNode::saveNodeState(juce::XmlElement *xml) {
  if (xml != nullptr) {
    xml->setAttribute("octaves", octaves);
    xml->setAttribute("macroOctaves", macroOctaves);
    xml->setAttribute("uniqueOnly", uniqueOnly);
  }
}

void OctaveStackNode::loadNodeState(juce::XmlElement *xml) {
  if (xml != nullptr) {
    octaves = xml->getIntAttribute("octaves", 1);
    macroOctaves = xml->getIntAttribute("macroOctaves", -1);
    uniqueOnly = xml->getBoolAttribute("uniqueOnly", true);
  }
}

void OctaveStackNode::process() {
  int actualOctaves =
      macroOctaves != -1 && macros[macroOctaves] != nullptr
          ? 1 + (int)std::round(macros[macroOctaves]->load() * 3.0f)
          : octaves;

  auto it = inputSequences.find(0);
  if (it == inputSequences.end() || it->second.empty()) {
    outputSequences[0] = NoteSequence();
  } else {
    NoteSequence outSeq;
    std::set<int> seenNotes;

    for (int oct = 0; oct < actualOctaves; ++oct) {
      for (const auto &step : it->second) {
        std::vector<HeldNote> outStep;
        for (const auto &note : step) {
          HeldNote n = note;
          n.noteNumber += (oct * 12);

          if (n.noteNumber <= 127) {
            if (!uniqueOnly ||
                seenNotes.find(n.noteNumber) == seenNotes.end()) {
              outStep.push_back(n);
              if (uniqueOnly) {
                seenNotes.insert(n.noteNumber);
              }
            }
          }
        }
        if (!outStep.empty()) {
          outSeq.push_back(outStep);
        }
      }
    }

    outputSequences[0] = outSeq;
  }

  auto connIt = connections.find(0);
  if (connIt != connections.end()) {
    for (const auto &connection : connIt->second) {
      connection.targetNode->setInputSequence(connection.targetInputPort,
                                              outputSequences[0]);
    }
  }
}
