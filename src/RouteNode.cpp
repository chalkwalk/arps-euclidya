#include "RouteNode.h"
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

class RouteNodeEditor : public juce::Component, private juce::Timer {
public:
  RouteNodeEditor(RouteNode &node, juce::AudioProcessorValueTreeState &)
      : routeNode(node) {

    updateButtonAppearance();
    addAndMakeVisible(button);

    button.onClick = [this]() {
      if (routeNode.macroRouteDest != -1)
        return;
      routeNode.routeDest = routeNode.routeDest == 0 ? 1 : 0;
      updateButtonAppearance();
      if (routeNode.onNodeDirtied)
        routeNode.onNodeDirtied();
    };

    button.onRightClick = [this, &btn = button,
                           &macroRef = routeNode.macroRouteDest]() {
      MacroMappingMenu::showMenu(&btn, macroRef,
                                 [this, &macroRef](int macroIndex) {
                                   macroRef = macroIndex;
                                   if (routeNode.onMappingChanged)
                                     routeNode.onMappingChanged();
                                 });
    };

    startTimerHz(30);
    setSize(160, 40);
  }

  ~RouteNodeEditor() override { stopTimer(); }

  void timerCallback() override {
    bool isMapped = routeNode.macroRouteDest != -1;
    bool currentVal;
    if (isMapped &&
        routeNode.macros[(size_t)routeNode.macroRouteDest] != nullptr) {
      currentVal =
          routeNode.macros[(size_t)routeNode.macroRouteDest]->load() >= 0.5f;
    } else {
      currentVal = routeNode.routeDest != 0;
    }

    if (currentVal != lastDisplayedVal || isMapped != wasMapped) {
      lastDisplayedVal = currentVal;
      wasMapped = isMapped;
      updateButtonAppearance();
      repaint();
      if (isMapped && routeNode.onNodeDirtied)
        routeNode.onNodeDirtied();
    }
  }

  void paint(juce::Graphics &g) override {
    if (routeNode.macroRouteDest != -1) {
      g.setColour(juce::Colours::orange.withAlpha(0.6f));
      g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1), 4.0f, 1.5f);
    }
  }

  void resized() override { button.setBounds(getLocalBounds().reduced(2)); }

private:
  void updateButtonAppearance() {
    bool isOut1 =
        (routeNode.macroRouteDest != -1 &&
         routeNode.macros[(size_t)routeNode.macroRouteDest] != nullptr)
            ? (routeNode.macros[(size_t)routeNode.macroRouteDest]->load() >=
               0.5f)
            : (routeNode.routeDest != 0);
    lastDisplayedVal = isOut1;
    button.setButtonText(isOut1 ? "Out 1" : "Out 0");
    button.setColour(juce::TextButton::buttonColourId,
                     isOut1 ? juce::Colour(0xff335566)
                            : juce::Colour(0xff444444));
  }

  RouteNode &routeNode;
  CustomMacroButton button;
  bool lastDisplayedVal = false;
  bool wasMapped = false;
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
