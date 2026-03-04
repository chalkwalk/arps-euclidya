#include "OctaveTransposeNode.h"
#include "MacroMappingMenu.h"
#include <algorithm>

class OctaveTransposeNodeEditor : public juce::Component {
public:
  OctaveTransposeNodeEditor(OctaveTransposeNode &node,
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
      slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
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

      slider.onRightClick = [&slider, &nodeMacroRef, &attachment, &apvts,
                             updateSliderVisibility]() {
        MacroMappingMenu::showMenu(
            &slider, nodeMacroRef,
            [&nodeMacroRef, &attachment, &apvts, &slider,
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
            });
      };

      // INIT
      if (nodeMacroRef != -1) {
        juce::String paramID = "macro_" + juce::String(nodeMacroRef + 1);
        attachment = std::make_unique<MacroAttachment>(apvts, paramID, slider);
      }
    };

    setupSlider(octavesSlider, octavesLabel, octaveNode.octaves,
                octaveNode.macroOctaves, octavesAttachment, "Octaves", -4, 4);

    setSize(400, 150);
  }

  void resized() override {
    auto bounds = getLocalBounds();
    int w = getWidth() / 3;
    auto b1 = bounds.removeFromLeft(w).removeFromTop(getHeight() / 2);

    auto bCopy = b1;
    octavesLabel.setBounds(bCopy.removeFromBottom(20));
    int size = std::min(bCopy.getWidth(), bCopy.getHeight());
    octavesSlider.setBounds(bCopy.withSizeKeepingCentre(size, size));
  }

private:
  OctaveTransposeNode &octaveNode;
  CustomMacroSlider octavesSlider;
  juce::Label octavesLabel;
  std::unique_ptr<MacroAttachment> octavesAttachment;
};

// --- OctaveTransposeNode Impl

OctaveTransposeNode::OctaveTransposeNode(
    std::array<std::atomic<float> *, 32> &inMacros)
    : macros(inMacros) {}

std::unique_ptr<juce::Component> OctaveTransposeNode::createEditorComponent(
    juce::AudioProcessorValueTreeState &apvts) {
  return std::make_unique<OctaveTransposeNodeEditor>(*this, apvts);
}

void OctaveTransposeNode::saveNodeState(juce::XmlElement *xml) {
  if (xml != nullptr) {
    xml->setAttribute("octaves", octaves);
    xml->setAttribute("macroOctaves", macroOctaves);
  }
}

void OctaveTransposeNode::loadNodeState(juce::XmlElement *xml) {
  if (xml != nullptr) {
    octaves = xml->getIntAttribute("octaves", 0);
    macroOctaves = xml->getIntAttribute("macroOctaves", -1);
  }
}

void OctaveTransposeNode::process() {
  int actualOctaves =
      macroOctaves != -1 && macros[macroOctaves] != nullptr
          ? -4 + (int)std::round(macros[macroOctaves]->load() * 8.0f)
          : octaves;

  auto it = inputSequences.find(0);
  if (it == inputSequences.end() || it->second.empty() || actualOctaves == 0) {
    // If no transposition needed, just pass through
    outputSequences[0] =
        it != inputSequences.end() ? it->second : NoteSequence();
  } else {
    NoteSequence outSeq;
    for (const auto &step : it->second) {
      std::vector<HeldNote> outStep;
      for (const auto &note : step) {
        HeldNote transposed = note;
        transposed.noteNumber += (actualOctaves * 12);
        if (transposed.noteNumber >= 0 && transposed.noteNumber <= 127) {
          outStep.push_back(transposed);
        }
      }
      if (!outStep.empty()) {
        outSeq.push_back(outStep);
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
