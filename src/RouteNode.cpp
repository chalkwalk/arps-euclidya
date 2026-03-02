#include "RouteNode.h"
#include "MacroMappingMenu.h"
#include <juce_gui_basics/juce_gui_basics.h>

class RouteNodeEditor : public juce::Component, private juce::Timer {
public:
  RouteNodeEditor(RouteNode &node, juce::AudioProcessorValueTreeState &)
      : routeNode(node) {

    toggle.setButtonText(routeNode.routeDest == 0 ? "Out 0" : "Out 1");
    toggle.setToggleState(routeNode.routeDest != 0, juce::dontSendNotification);
    toggle.setClickingTogglesState(true);
    toggle.onClick = [this]() {
      routeNode.routeDest = toggle.getToggleState() ? 1 : 0;
      toggle.setButtonText(routeNode.routeDest == 0 ? "Out 0" : "Out 1");
      if (routeNode.onNodeDirtied)
        routeNode.onNodeDirtied();
    };
    addAndMakeVisible(toggle);

    toggle.addMouseListener(this, true);

    if (routeNode.macroRouteDest != -1)
      startTimerHz(10);

    setSize(160, 40);
  }

  void mouseDown(const juce::MouseEvent &e) override {
    if (e.mods.isRightButtonDown()) {
      MacroMappingMenu::showMenu(&toggle, routeNode.macroRouteDest,
                                 [this](int macroIndex) {
                                   routeNode.macroRouteDest = macroIndex;
                                   if (macroIndex != -1)
                                     startTimerHz(10);
                                   else
                                     stopTimer();
                                   repaint();
                                   if (routeNode.onNodeDirtied)
                                     routeNode.onNodeDirtied();
                                 });
    }
  }

  void timerCallback() override {
    if (routeNode.macroRouteDest != -1 &&
        routeNode.macros[(size_t)routeNode.macroRouteDest] != nullptr) {
      bool val =
          routeNode.macros[(size_t)routeNode.macroRouteDest]->load() >= 0.5f;
      toggle.setToggleState(val, juce::dontSendNotification);
      toggle.setButtonText(val ? "Out 1" : "Out 0");
    }
  }

  void paint(juce::Graphics &g) override {
    if (routeNode.macroRouteDest != -1) {
      g.setColour(juce::Colours::orange.withAlpha(0.6f));
      g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1), 4.0f, 1.5f);
    }
  }

  void resized() override { toggle.setBounds(getLocalBounds().reduced(2)); }

private:
  RouteNode &routeNode;
  juce::ToggleButton toggle;
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
          ? (macros[(size_t)macroRouteDest]->load() >= 0.5f ? 1 : 0)
          : routeDest;

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
