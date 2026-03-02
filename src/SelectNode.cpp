#include "SelectNode.h"
#include "MacroMappingMenu.h"
#include <juce_gui_basics/juce_gui_basics.h>

class SelectNodeEditor : public juce::Component, private juce::Timer {
public:
  SelectNodeEditor(SelectNode &node, juce::AudioProcessorValueTreeState &)
      : selectNode(node) {

    toggle.setButtonText(selectNode.selectSource == 0 ? "In 0" : "In 1");
    toggle.setToggleState(selectNode.selectSource != 0,
                          juce::dontSendNotification);
    toggle.setClickingTogglesState(true);
    toggle.onClick = [this]() {
      selectNode.selectSource = toggle.getToggleState() ? 1 : 0;
      toggle.setButtonText(selectNode.selectSource == 0 ? "In 0" : "In 1");
      if (selectNode.onNodeDirtied)
        selectNode.onNodeDirtied();
    };
    addAndMakeVisible(toggle);

    toggle.addMouseListener(this, true);

    if (selectNode.macroSelectSource != -1)
      startTimerHz(10);

    setSize(160, 40);
  }

  void mouseDown(const juce::MouseEvent &e) override {
    if (e.mods.isRightButtonDown()) {
      MacroMappingMenu::showMenu(&toggle, selectNode.macroSelectSource,
                                 [this](int macroIndex) {
                                   selectNode.macroSelectSource = macroIndex;
                                   if (macroIndex != -1)
                                     startTimerHz(10);
                                   else
                                     stopTimer();
                                   repaint();
                                   if (selectNode.onNodeDirtied)
                                     selectNode.onNodeDirtied();
                                 });
    }
  }

  void timerCallback() override {
    if (selectNode.macroSelectSource != -1 &&
        selectNode.macros[(size_t)selectNode.macroSelectSource] != nullptr) {
      bool val =
          selectNode.macros[(size_t)selectNode.macroSelectSource]->load() >=
          0.5f;
      toggle.setToggleState(val, juce::dontSendNotification);
      toggle.setButtonText(val ? "In 1" : "In 0");
    }
  }

  void paint(juce::Graphics &g) override {
    if (selectNode.macroSelectSource != -1) {
      g.setColour(juce::Colours::orange.withAlpha(0.6f));
      g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1), 4.0f, 1.5f);
    }
  }

  void resized() override { toggle.setBounds(getLocalBounds().reduced(2)); }

private:
  SelectNode &selectNode;
  juce::ToggleButton toggle;
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
