#include "SwitchNode.h"
#include "MacroMappingMenu.h"
#include <juce_gui_basics/juce_gui_basics.h>

// Custom button with right-click handler, mirroring CustomMacroSlider
class CustomMacroButton : public juce::TextButton {
public:
  std::function<void()> onRightClick;

  void mouseDown(const juce::MouseEvent &e) override {
    if (e.mods.isPopupMenu()) {
      if (onRightClick)
        onRightClick();
    } else {
      juce::TextButton::mouseDown(e);
    }
  }
};

class SwitchNodeEditor : public juce::Component, private juce::Timer {
public:
  SwitchNodeEditor(SwitchNode &node, juce::AudioProcessorValueTreeState &)
      : switchNode(node) {

    updateButtonAppearance();
    addAndMakeVisible(button);

    // Left-click: toggle value (ignored when macro-mapped)
    button.onClick = [this]() {
      if (switchNode.macroSwitch != -1)
        return;
      switchNode.switchOn = !switchNode.switchOn;
      updateButtonAppearance();
      if (switchNode.onNodeDirtied)
        switchNode.onNodeDirtied();
    };

    // Right-click: macro mapping menu
    // Mirror the rotary slider pattern: capture only specific member refs,
    // NEVER capture [this]. Let the timer handle UI updates.
    button.onRightClick = [&btn = button,
                           &macroRef = switchNode.macroSwitch]() {
      MacroMappingMenu::showMenu(&btn, macroRef, [&macroRef](int macroIndex) {
        macroRef = macroIndex;
      });
    };

    startTimerHz(30);
    setSize(160, 40);
  }

  ~SwitchNodeEditor() override { stopTimer(); }

  // Timer acts like MacroAttachment for toggles:
  // polls the macro value and triggers onNodeDirtied when it changes
  void timerCallback() override {
    bool isMapped = switchNode.macroSwitch != -1;
    bool currentVal;
    if (isMapped &&
        switchNode.macros[(size_t)switchNode.macroSwitch] != nullptr) {
      currentVal =
          switchNode.macros[(size_t)switchNode.macroSwitch]->load() >= 0.5f;
    } else {
      currentVal = switchNode.switchOn;
    }

    if (currentVal != lastDisplayedVal || isMapped != wasMapped) {
      lastDisplayedVal = currentVal;
      wasMapped = isMapped;
      updateButtonAppearance();
      repaint();
      // Trigger graph recalculation when macro value changes
      if (isMapped && switchNode.onNodeDirtied)
        switchNode.onNodeDirtied();
    }
  }

  void paint(juce::Graphics &g) override {
    if (switchNode.macroSwitch != -1) {
      g.setColour(juce::Colours::orange.withAlpha(0.6f));
      g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1), 4.0f, 1.5f);
    }
  }

  void resized() override { button.setBounds(getLocalBounds().reduced(2)); }

private:
  void updateButtonAppearance() {
    bool currentVal =
        (switchNode.macroSwitch != -1 &&
         switchNode.macros[(size_t)switchNode.macroSwitch] != nullptr)
            ? (switchNode.macros[(size_t)switchNode.macroSwitch]->load() >=
               0.5f)
            : switchNode.switchOn;
    lastDisplayedVal = currentVal;
    button.setButtonText(currentVal ? "On" : "Off");
    button.setColour(juce::TextButton::buttonColourId,
                     currentVal ? juce::Colour(0xff336633)
                                : juce::Colour(0xff663333));
  }

  SwitchNode &switchNode;
  CustomMacroButton button;
  bool lastDisplayedVal = false;
  bool wasMapped = false;
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
