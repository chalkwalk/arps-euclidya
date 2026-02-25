#include "SwitchNode.h"
#include "MacroMappingMenu.h"
#include <juce_gui_basics/juce_gui_basics.h>

class SwitchNodeEditor : public juce::Component {
public:
  SwitchNodeEditor(SwitchNode &node, juce::AudioProcessorValueTreeState &apvts)
      : switchNode(node) {

    toggle.setButtonText("Pass-through On");
    toggle.setToggleState(switchNode.switchOn, juce::dontSendNotification);
    toggle.onClick = [this]() {
      switchNode.switchOn = toggle.getToggleState();
      if (switchNode.onNodeDirtied)
        switchNode.onNodeDirtied();
    };
    addAndMakeVisible(toggle);

    auto updateVisibility = [this](int macro) {
      if (macro == -1) {
        toggle.setAlpha(1.0f);
        toggle.setEnabled(true);
      } else {
        toggle.setAlpha(0.5f);
        toggle.setEnabled(false);
      }
    };

    updateVisibility(switchNode.macroSwitch);

    toggle.addMouseListener(this, true);

    setSize(160, 40);
  }

  void mouseDown(const juce::MouseEvent &e) override {
    if (e.mods.isRightButtonDown()) {
      MacroMappingMenu::showMenu(&toggle, switchNode.macroSwitch,
                                 [this](int macroIndex) {
                                   switchNode.macroSwitch = macroIndex;
                                   if (macroIndex == -1) {
                                     toggle.setAlpha(1.0f);
                                     toggle.setEnabled(true);
                                   } else {
                                     toggle.setAlpha(0.5f);
                                     toggle.setEnabled(false);
                                   }
                                   if (switchNode.onNodeDirtied)
                                     switchNode.onNodeDirtied();
                                 });
    }
  }

  void resized() override { toggle.setBounds(getLocalBounds().reduced(2)); }

private:
  SwitchNode &switchNode;
  juce::ToggleButton toggle;
};

SwitchNode::SwitchNode(std::array<std::atomic<float> *, 32> &inMacros)
    : macros(inMacros) {}

std::unique_ptr<juce::Component>
SwitchNode::createEditorComponent(juce::AudioProcessorValueTreeState &apvts) {
  return std::make_unique<SwitchNodeEditor>(*this, apvts);
}

void SwitchNode::saveNodeState(juce::XmlElement *xml) {
  if (xml) {
    xml->setAttribute("switchOn", switchOn);
    xml->setAttribute("macroSwitch", macroSwitch);
  }
}

void SwitchNode::loadNodeState(juce::XmlElement *xml) {
  if (xml) {
    switchOn = xml->getBoolAttribute("switchOn", true);
    macroSwitch = xml->getIntAttribute("macroSwitch", -1);
  }
}

void SwitchNode::process() {
  bool actualOn = macroSwitch != -1 && macros[(size_t)macroSwitch] != nullptr
                      ? (macros[(size_t)macroSwitch]->load() > 0.5f)
                      : switchOn;

  auto it = inputSequences.find(0);
  NoteSequence inSeq =
      (it != inputSequences.end()) ? it->second : NoteSequence();

  if (actualOn) {
    outputSequences[0] = inSeq;
  } else {
    // Fill with rests of the same length if empty sequence is not desired?
    // "not (an empty sequence)" per step 22: "pass through input, or not (an
    // empty sequence)".
    outputSequences[0] = NoteSequence();
  }

  auto conn = connections.find(0);
  if (conn != connections.end()) {
    for (const auto &c : conn->second) {
      c.targetNode->setInputSequence(c.targetInputPort, outputSequences[0]);
    }
  }
}
