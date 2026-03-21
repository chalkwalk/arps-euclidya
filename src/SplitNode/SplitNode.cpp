#include "SplitNode.h"

void SplitNode::process() {
  auto it = inputSequences.find(0);
  if (it == inputSequences.end() || it->second.empty()) {
    outputSequences[0] = {};
    outputSequences[1] = {};
    return;
  }

  const auto &seq = it->second;
  NoteSequence out0, out1;

  switch (splitMode) {
  case 0: { // First half / Second half
    size_t mid = seq.size() / 2;
    if (mid == 0)
      mid = 1;
    for (size_t i = 0; i < seq.size(); ++i) {
      if (i < mid)
        out0.push_back(seq[i]);
      else
        out1.push_back(seq[i]);
    }
    break;
  }
  case 1: { // Odd / Even indexed
    for (size_t i = 0; i < seq.size(); ++i) {
      if (i % 2 == 0)
        out0.push_back(seq[i]);
      else
        out1.push_back(seq[i]);
    }
    break;
  }
  case 2: { // Percentage
    size_t splitAt = std::max(
        (size_t)1, (size_t)std::round(seq.size() * splitPercent / 100.0));
    splitAt = std::min(splitAt, seq.size());
    for (size_t i = 0; i < seq.size(); ++i) {
      if (i < splitAt)
        out0.push_back(seq[i]);
      else
        out1.push_back(seq[i]);
    }
    break;
  }
  case 3: { // First: first step → out0, rest → out1
    out0.push_back(seq.front());
    for (size_t i = 1; i < seq.size(); ++i)
      out1.push_back(seq[i]);
    break;
  }
  case 4: { // Last: last step → out1, rest → out0
    for (size_t i = 0; i + 1 < seq.size(); ++i)
      out0.push_back(seq[i]);
    out1.push_back(seq.back());
    break;
  }
  default:
    out0 = seq;
    break;
  }

  // Ensure at least one rest step if empty
  if (out0.empty())
    out0.push_back({});
  if (out1.empty())
    out1.push_back({});

  outputSequences[0] = out0;
  outputSequences[1] = out1;
}

void SplitNode::saveNodeState(juce::XmlElement *xml) {
  if (xml) {
    xml->setAttribute("splitMode", splitMode);
    xml->setAttribute("splitPercent", splitPercent);
  }
}

void SplitNode::loadNodeState(juce::XmlElement *xml) {
  if (xml) {
    splitMode = xml->getIntAttribute("splitMode", 0);
    splitPercent = xml->getIntAttribute("splitPercent", 50);
  }
}

// --- Editor ---
class SplitNodeEditor : public juce::Component {
public:
  SplitNodeEditor(SplitNode &node) : splitNode(node) {
    modeBox.addItem("First/Last Half", 1);
    modeBox.addItem("Odd/Even", 2);
    modeBox.addItem("Percentage", 3);
    modeBox.addItem("First", 4);
    modeBox.addItem("Last", 5);
    modeBox.setSelectedId(node.splitMode + 1, juce::dontSendNotification);
    modeBox.onChange = [this]() {
      splitNode.splitMode = modeBox.getSelectedId() - 1;
      percentSlider.setVisible(splitNode.splitMode == 2);
      if (splitNode.onNodeDirtied)
        splitNode.onNodeDirtied();
    };
    addAndMakeVisible(modeBox);

    percentSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    percentSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
    percentSlider.setRange(1, 99, 1);
    percentSlider.setValue(node.splitPercent);
    percentSlider.onValueChange = [this]() {
      splitNode.splitPercent = (int)percentSlider.getValue();
      if (splitNode.onNodeDirtied)
        splitNode.onNodeDirtied();
    };
    percentSlider.setVisible(node.splitMode == 2);
    addAndMakeVisible(percentSlider);

    setSize(260, 60);
  }

  void resized() override {
    auto bounds = getLocalBounds().reduced(10);

    // Mode box in top half
    modeBox.setBounds(bounds.removeFromTop(30).reduced(2));

    // Percentage slider in bottom half (if visible)
    if (percentSlider.isVisible()) {
      bounds.removeFromTop(10); // spacing
      percentSlider.setBounds(bounds.removeFromTop(30).reduced(2));
    }
  }

private:
  SplitNode &splitNode;
  juce::ComboBox modeBox;
  juce::Slider percentSlider;
};

std::unique_ptr<juce::Component>
SplitNode::createEditorComponent(juce::AudioProcessorValueTreeState &) {
  return std::make_unique<SplitNodeEditor>(*this);
}
