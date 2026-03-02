#include "SwitchNode.h"
#include "MacroMappingMenu.h"
#include <juce_gui_basics/juce_gui_basics.h>

class SwitchNodeEditor : public juce::Component, private juce::Timer {
public:
  SwitchNodeEditor(SwitchNode &node, juce::AudioProcessorValueTreeState &)
      : switchNode(node) {

    toggle.setButtonText(switchNode.switchOn ? "On" : "Off");
    toggle.setToggleState(switchNode.switchOn, juce::dontSendNotification);
    toggle.setClickingTogglesState(true);
    toggle.onClick = [this]() {
      switchNode.switchOn = toggle.getToggleState();
      toggle.setButtonText(switchNode.switchOn ? "On" : "Off");
      if (switchNode.onNodeDirtied)
        switchNode.onNodeDirtied();
    };
    addAndMakeVisible(toggle);

    toggle.addMouseListener(this, true);

    if (switchNode.macroSwitch != -1)
      startTimerHz(10);

    setSize(160, 40);
  }

  void mouseDown(const juce::MouseEvent &e) override {
    if (e.mods.isRightButtonDown()) {
      MacroMappingMenu::showMenu(&toggle, switchNode.macroSwitch,
                                 [this](int macroIndex) {
                                   switchNode.macroSwitch = macroIndex;
                                   if (macroIndex != -1)
                                     startTimerHz(10);
                                   else
                                     stopTimer();
                                   repaint();
                                   if (switchNode.onNodeDirtied)
                                     switchNode.onNodeDirtied();
                                 });
    }
  }

  void timerCallback() override {
    if (switchNode.macroSwitch != -1 &&
        switchNode.macros[(size_t)switchNode.macroSwitch] != nullptr) {
      bool val =
          switchNode.macros[(size_t)switchNode.macroSwitch]->load() >= 0.5f;
      toggle.setToggleState(val, juce::dontSendNotification);
      toggle.setButtonText(val ? "On" : "Off");
    }
  }

  void paint(juce::Graphics &g) override {
    if (switchNode.macroSwitch != -1) {
      g.setColour(juce::Colours::orange.withAlpha(0.6f));
      g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1), 4.0f, 1.5f);
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
                      ? (macros[(size_t)macroSwitch]->load() >= 0.5f)
                      : switchOn;

  auto it = inputSequences.find(0);
  NoteSequence inSeq =
      (it != inputSequences.end()) ? it->second : NoteSequence();

  if (actualOn) {
    outputSequences[0] = inSeq;
  } else {
    outputSequences[0] = NoteSequence();
  }

  auto conn = connections.find(0);
  if (conn != connections.end()) {
    for (const auto &c : conn->second) {
      c.targetNode->setInputSequence(c.targetInputPort, outputSequences[0]);
    }
  }
}
