#include "MidiInNode.h"
#include "MacroMappingMenu.h"
#include "SharedMacroUI.h"
#include <juce_gui_basics/juce_gui_basics.h>

class MidiInNodeEditor : public juce::Component {
public:
  MidiInNodeEditor(MidiInNode &node, juce::AudioProcessorValueTreeState &apvts)
      : midiInNode(node) {

    auto setupSlider = [this, &apvts](
                           CustomMacroSlider &slider, juce::Label &label,
                           int &nodeValueRef, int &nodeMacroRef,
                           std::unique_ptr<MacroAttachment> &attachment,
                           const juce::String &text, int minVal, int maxVal) {
      slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
      slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
      slider.setRange(minVal, maxVal, 1);
      slider.setValue(nodeValueRef);
      addAndMakeVisible(slider);

      label.setText(text, juce::dontSendNotification);
      label.setJustificationType(juce::Justification::centred);
      addAndMakeVisible(label);

      slider.onValueChange = [this, &slider, &nodeValueRef]() {
        nodeValueRef = (int)slider.getValue();
        midiInNode.getMidiHandler().forceDirty();
        if (midiInNode.onNodeDirtied)
          midiInNode.onNodeDirtied();
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

      // Initialize tooltip and attachment if previously mapped
      if (nodeMacroRef != -1) {
        slider.setTooltip("Mapped to Macro " + juce::String(nodeMacroRef + 1));
        attachment = std::make_unique<MacroAttachment>(
            apvts, "macro_" + juce::String(nodeMacroRef + 1), slider);
      }
      updateSliderVisibility(nodeMacroRef);
    };

    setupSlider(channelFilterSlider, channelFilterLabel,
                midiInNode.channelFilter, midiInNode.macroChannelFilter,
                channelFilterAttachment, "Channel Filter (0=All)", 0, 16);

    legacyModeToggle.setButtonText("Legacy Mode (Non-MPE)");
    legacyModeToggle.setToggleState(midiInNode.legacyMode,
                                    juce::dontSendNotification);
    legacyModeToggle.onClick = [this]() {
      midiInNode.legacyMode = legacyModeToggle.getToggleState();
      midiInNode.getMidiHandler().setLegacyMode(midiInNode.legacyMode);
      midiInNode.getMidiHandler().forceDirty();
      if (midiInNode.onNodeDirtied)
        midiInNode.onNodeDirtied();
    };
    addAndMakeVisible(legacyModeToggle);

    setSize(400, 150);
  }

  void resized() override {
    auto bounds = getLocalBounds();

    // Create a 1/3rd width bounding box aligned to top-left to emulate Rank
    // Order grid spacing
    int w = getWidth() / 3;
    auto b1 = bounds.removeFromLeft(w).removeFromTop(getHeight() / 2);

    auto bCopy = b1;
    channelFilterLabel.setBounds(bCopy.removeFromBottom(20));
    int size = std::min(bCopy.getWidth(), bCopy.getHeight());
    channelFilterSlider.setBounds(bCopy.withSizeKeepingCentre(size, size));

    auto b2 = bounds.removeFromLeft(w).removeFromTop(getHeight() / 2);
    legacyModeToggle.setBounds(b2.reduced(10));
  }

private:
  MidiInNode &midiInNode;
  CustomMacroSlider channelFilterSlider;
  juce::Label channelFilterLabel;
  std::unique_ptr<MacroAttachment> channelFilterAttachment;
  juce::ToggleButton legacyModeToggle;
};

MidiInNode::MidiInNode(MidiHandler &handler,
                       std::array<std::atomic<float> *, 32> macrosArray)
    : midiHandler(handler), macros(macrosArray) {}

std::unique_ptr<juce::Component>
MidiInNode::createEditorComponent(juce::AudioProcessorValueTreeState &apvts) {
  return std::make_unique<MidiInNodeEditor>(*this, apvts);
}

void MidiInNode::process() {
  if (midiHandler.isLegacyModeEnabled() != legacyMode) {
    midiHandler.setLegacyMode(legacyMode);
  }

  int actualChannelFilter =
      macroChannelFilter != -1 && macros[macroChannelFilter] != nullptr
          ? (int)std::round(macros[macroChannelFilter]->load() * 16.0f)
          : channelFilter;

  auto heldNotes = midiHandler.getHeldNotes(actualChannelFilter);

  NoteSequence seq;

  // Convert 1D list of HeldNotes into 2D NoteSequence where each step is one
  // note
  for (const auto &note : heldNotes) {
    std::vector<HeldNote> step = {note};
    seq.push_back(step);
  }

  // Cache the sequence at output port 0
  outputSequences[0] = seq;

  // Push the sequence downstream to all connected nodes
  auto it = connections.find(0);
  if (it != connections.end()) {
    for (const auto &connection : it->second) {
      connection.targetNode->setInputSequence(connection.targetInputPort, seq);
    }
  }
}

void MidiInNode::saveNodeState(juce::XmlElement *xml) {
  if (xml != nullptr) {
    xml->setAttribute("channelFilter", channelFilter);
    xml->setAttribute("macroChannelFilter", macroChannelFilter);
    xml->setAttribute("legacyMode", legacyMode);
  }
}

void MidiInNode::loadNodeState(juce::XmlElement *xml) {
  if (xml != nullptr) {
    channelFilter = xml->getIntAttribute("channelFilter", 0);
    macroChannelFilter = xml->getIntAttribute("macroChannelFilter", -1);
    legacyMode = xml->getBoolAttribute("legacyMode", false);
    midiHandler.setLegacyMode(legacyMode);
    midiHandler.forceDirty();
  }
}
