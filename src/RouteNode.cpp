#include "RouteNode.h"
#include "MacroMappingMenu.h"
#include <juce_gui_basics/juce_gui_basics.h>

class RouteNodeEditor : public juce::Component {
public:
  RouteNodeEditor(RouteNode &node, juce::AudioProcessorValueTreeState &apvts)
      : routeNode(node) {

    destBox.addItem("Out 0", 1);
    destBox.addItem("Out 1", 2);
    destBox.setSelectedId(routeNode.routeDest + 1, juce::dontSendNotification);
    destBox.onChange = [this]() {
      routeNode.routeDest = destBox.getSelectedId() - 1;
      if (routeNode.onNodeDirtied)
        routeNode.onNodeDirtied();
    };
    addAndMakeVisible(destBox);

    auto updateVisibility = [this](int macro) {
      if (macro == -1) {
        destBox.setAlpha(1.0f);
        destBox.setEnabled(true);
      } else {
        destBox.setAlpha(0.5f);
        destBox.setEnabled(false);
      }
    };

    updateVisibility(routeNode.macroRouteDest);

    destBox.addMouseListener(this, true);

    setSize(160, 40);
  }

  void mouseDown(const juce::MouseEvent &e) override {
    if (e.mods.isRightButtonDown()) {
      MacroMappingMenu::showMenu(&destBox, routeNode.macroRouteDest,
                                 [this](int macroIndex) {
                                   routeNode.macroRouteDest = macroIndex;
                                   if (macroIndex == -1) {
                                     destBox.setAlpha(1.0f);
                                     destBox.setEnabled(true);
                                   } else {
                                     destBox.setAlpha(0.5f);
                                     destBox.setEnabled(false);
                                   }
                                   if (routeNode.onNodeDirtied)
                                     routeNode.onNodeDirtied();
                                 });
    }
  }

  void resized() override { destBox.setBounds(getLocalBounds().reduced(2)); }

private:
  RouteNode &routeNode;
  juce::ComboBox destBox;
};

RouteNode::RouteNode(std::array<std::atomic<float> *, 32> &inMacros)
    : macros(inMacros) {}

std::unique_ptr<juce::Component>
RouteNode::createEditorComponent(juce::AudioProcessorValueTreeState &apvts) {
  return std::make_unique<RouteNodeEditor>(*this, apvts);
}

void RouteNode::saveNodeState(juce::XmlElement *xml) {
  if (xml) {
    xml->setAttribute("routeDest", routeDest);
    xml->setAttribute("macroRouteDest", macroRouteDest);
  }
}

void RouteNode::loadNodeState(juce::XmlElement *xml) {
  if (xml) {
    routeDest = xml->getIntAttribute("routeDest", 0);
    macroRouteDest = xml->getIntAttribute("macroRouteDest", -1);
  }
}

void RouteNode::process() {
  int actualDest =
      macroRouteDest != -1 && macros[(size_t)macroRouteDest] != nullptr
          ? (int)std::round(macros[(size_t)macroRouteDest]->load())
          : routeDest;

  actualDest = std::clamp(actualDest, 0, 1);

  auto it = inputSequences.find(0);
  NoteSequence emptySeq;
  NoteSequence inSeq = (it != inputSequences.end()) ? it->second : emptySeq;

  if (actualDest == 0) {
    outputSequences[0] = inSeq;
    outputSequences[1] = emptySeq;
  } else {
    outputSequences[0] = emptySeq;
    outputSequences[1] = inSeq;
  }

  auto conn0 = connections.find(0);
  if (conn0 != connections.end()) {
    for (const auto &c : conn0->second) {
      c.targetNode->setInputSequence(c.targetInputPort, outputSequences[0]);
    }
  }

  auto conn1 = connections.find(1);
  if (conn1 != connections.end()) {
    for (const auto &c : conn1->second) {
      c.targetNode->setInputSequence(c.targetInputPort, outputSequences[1]);
    }
  }
}
