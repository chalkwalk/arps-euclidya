#include "TransposeNode.h"
#include "MacroMappingMenu.h"
#include <algorithm>

class TransposeNodeEditor : public juce::Component {
public:
  TransposeNodeEditor(TransposeNode &node,
                      juce::AudioProcessorValueTreeState &apvts)
      : transposeNode(node) {

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
        if (transposeNode.onNodeDirtied)
          transposeNode.onNodeDirtied();
      };

      auto updateSliderVisibility = [&slider](int macro) {
        if (macro == -1) {
          slider.setAlpha(1.0f);
          slider.setEnabled(true);
        } else {
          slider.setAlpha(0.5f);
          slider.setEnabled(false);
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

    setupSlider(semitonesSlider, semitonesLabel, transposeNode.semitones,
                transposeNode.macroSemitones, semitonesAttachment, "Semitones",
                -24, 24);

    setSize(400, 150);
  }

  void resized() override {
    auto bounds = getLocalBounds();
    int w = getWidth() / 3;
    auto b1 = bounds.removeFromLeft(w).removeFromTop(getHeight() / 2);

    auto bCopy = b1;
    semitonesLabel.setBounds(bCopy.removeFromBottom(20));
    int size = std::min(bCopy.getWidth(), bCopy.getHeight());
    semitonesSlider.setBounds(bCopy.withSizeKeepingCentre(size, size));
  }

private:
  TransposeNode &transposeNode;
  CustomMacroSlider semitonesSlider;
  juce::Label semitonesLabel;
  std::unique_ptr<MacroAttachment> semitonesAttachment;
};

// --- TransposeNode Impl

TransposeNode::TransposeNode(std::array<std::atomic<float> *, 32> &inMacros)
    : macros(inMacros) {}

std::unique_ptr<juce::Component> TransposeNode::createEditorComponent(
    juce::AudioProcessorValueTreeState &apvts) {
  return std::make_unique<TransposeNodeEditor>(*this, apvts);
}

void TransposeNode::saveNodeState(juce::XmlElement *xml) {
  if (xml != nullptr) {
    xml->setAttribute("semitones", semitones);
    xml->setAttribute("macroSemitones", macroSemitones);
  }
}

void TransposeNode::loadNodeState(juce::XmlElement *xml) {
  if (xml != nullptr) {
    semitones = xml->getIntAttribute("semitones", 0);
    macroSemitones = xml->getIntAttribute("macroSemitones", -1);
  }
}

void TransposeNode::process() {
  int actualSemitones =
      macroSemitones != -1 && macros[macroSemitones] != nullptr
          ? -24 + (int)std::round(macros[macroSemitones]->load() * 48.0f)
          : semitones;

  auto it = inputSequences.find(0);
  if (it == inputSequences.end() || it->second.empty() ||
      actualSemitones == 0) {
    // If no transposition needed, just pass through
    outputSequences[0] =
        it != inputSequences.end() ? it->second : NoteSequence();
  } else {
    NoteSequence outSeq;
    for (const auto &step : it->second) {
      std::vector<HeldNote> outStep;
      for (const auto &note : step) {
        HeldNote transposed = note;
        transposed.noteNumber += actualSemitones;
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
