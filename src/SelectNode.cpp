#include "SelectNode.h"
#include "MacroMappingMenu.h"
#include <juce_gui_basics/juce_gui_basics.h>

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

class SelectNodeEditor : public juce::Component, private juce::Timer {
public:
  SelectNodeEditor(SelectNode &node, juce::AudioProcessorValueTreeState &)
      : selectNode(node) {

    updateButtonAppearance();
    addAndMakeVisible(button);

    button.onClick = [this]() {
      if (selectNode.macroSelectSource != -1)
        return;
      selectNode.selectSource = selectNode.selectSource == 0 ? 1 : 0;
      updateButtonAppearance();
      if (selectNode.onNodeDirtied)
        selectNode.onNodeDirtied();
    };

    button.onRightClick = [this, &btn = button,
                           &macroRef = selectNode.macroSelectSource]() {
      MacroMappingMenu::showMenu(&btn, macroRef,
                                 [this, &macroRef](int macroIndex) {
                                   macroRef = macroIndex;
                                   if (selectNode.onMappingChanged)
                                     selectNode.onMappingChanged();
                                 });
    };

    startTimerHz(30);
    setSize(160, 40);
  }

  ~SelectNodeEditor() override { stopTimer(); }

  void timerCallback() override {
    bool isMapped = selectNode.macroSelectSource != -1;
    bool currentVal;
    if (isMapped &&
        selectNode.macros[(size_t)selectNode.macroSelectSource] != nullptr) {
      currentVal =
          selectNode.macros[(size_t)selectNode.macroSelectSource]->load() >=
          0.5f;
    } else {
      currentVal = selectNode.selectSource != 0;
    }

    if (currentVal != lastDisplayedVal || isMapped != wasMapped) {
      lastDisplayedVal = currentVal;
      wasMapped = isMapped;
      updateButtonAppearance();
      repaint();
      if (isMapped && selectNode.onNodeDirtied)
        selectNode.onNodeDirtied();
    }
  }

  void paint(juce::Graphics &g) override {
    if (selectNode.macroSelectSource != -1) {
      g.setColour(juce::Colours::orange.withAlpha(0.6f));
      g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1), 4.0f, 1.5f);
    }
  }

  void resized() override { button.setBounds(getLocalBounds().reduced(2)); }

private:
  void updateButtonAppearance() {
    bool isIn1 =
        (selectNode.macroSelectSource != -1 &&
         selectNode.macros[(size_t)selectNode.macroSelectSource] != nullptr)
            ? (selectNode.macros[(size_t)selectNode.macroSelectSource]
                   ->load() >= 0.5f)
            : (selectNode.selectSource != 0);
    lastDisplayedVal = isIn1;
    button.setButtonText(isIn1 ? "In 1" : "In 0");
    button.setColour(juce::TextButton::buttonColourId,
                     isIn1 ? juce::Colour(0xff335566)
                           : juce::Colour(0xff444444));
  }

  SelectNode &selectNode;
  CustomMacroButton button;
  bool lastDisplayedVal = false;
  bool wasMapped = false;
};

SelectNode::SelectNode(std::array<std::atomic<float> *, 32> &inMacros)
    : macros(inMacros) {}

std::unique_ptr<juce::Component>
SelectNode::createEditorComponent(juce::AudioProcessorValueTreeState &apvts) {
  return std::make_unique<SelectNodeEditor>(*this, apvts);
}

void SelectNode::saveNodeState(juce::XmlElement *xml) {
  if (xml) {
    xml->setAttribute("selectSource", selectSource);
    xml->setAttribute("macroSelectSource", macroSelectSource);
  }
}

void SelectNode::loadNodeState(juce::XmlElement *xml) {
  if (xml) {
    selectSource = xml->getIntAttribute("selectSource", 0);
    macroSelectSource = xml->getIntAttribute("macroSelectSource", -1);
  }
}

void SelectNode::process() {
  int actualSrc =
      macroSelectSource != -1 && macros[(size_t)macroSelectSource] != nullptr
          ? (macros[(size_t)macroSelectSource]->load() >= 0.5f ? 1 : 0)
          : selectSource;

  auto it = inputSequences.find(actualSrc);
  NoteSequence outSeq =
      (it != inputSequences.end()) ? it->second : NoteSequence();

  outputSequences[0] = outSeq;

  auto conn = connections.find(0);
  if (conn != connections.end()) {
    for (const auto &c : conn->second) {
      c.targetNode->setInputSequence(c.targetInputPort, outputSequences[0]);
    }
  }
}
