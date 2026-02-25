#include "SelectNode.h"
#include "MacroMappingMenu.h"
#include <juce_gui_basics/juce_gui_basics.h>

class SelectNodeEditor : public juce::Component {
public:
  SelectNodeEditor(SelectNode &node, juce::AudioProcessorValueTreeState &apvts)
      : selectNode(node) {

    sourceBox.addItem("In 0", 1);
    sourceBox.addItem("In 1", 2);
    sourceBox.setSelectedId(selectNode.selectSource + 1,
                            juce::dontSendNotification);
    sourceBox.onChange = [this]() {
      selectNode.selectSource = sourceBox.getSelectedId() - 1;
      if (selectNode.onNodeDirtied)
        selectNode.onNodeDirtied();
    };
    addAndMakeVisible(sourceBox);

    auto updateVisibility = [this](int macro) {
      if (macro == -1) {
        sourceBox.setAlpha(1.0f);
        sourceBox.setEnabled(true);
      } else {
        sourceBox.setAlpha(0.5f);
        sourceBox.setEnabled(false);
      }
    };

    updateVisibility(selectNode.macroSelectSource);

    sourceBox.addMouseListener(this, true);

    setSize(160, 40);
  }

  void mouseDown(const juce::MouseEvent &e) override {
    if (e.mods.isRightButtonDown()) {
      MacroMappingMenu::showMenu(&sourceBox, selectNode.macroSelectSource,
                                 [this](int macroIndex) {
                                   selectNode.macroSelectSource = macroIndex;
                                   if (macroIndex == -1) {
                                     sourceBox.setAlpha(1.0f);
                                     sourceBox.setEnabled(true);
                                   } else {
                                     sourceBox.setAlpha(0.5f);
                                     sourceBox.setEnabled(false);
                                   }
                                   if (selectNode.onNodeDirtied)
                                     selectNode.onNodeDirtied();
                                 });
    }
  }

  void resized() override { sourceBox.setBounds(getLocalBounds().reduced(2)); }

private:
  SelectNode &selectNode;
  juce::ComboBox sourceBox;
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
          ? (int)std::round(macros[(size_t)macroSelectSource]->load())
          : selectSource;

  actualSrc = std::clamp(actualSrc, 0, 1);

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
