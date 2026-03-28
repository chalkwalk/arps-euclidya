#include "DiagnosticNode.h"

#include "../LayoutParser.h"
#include "BinaryData.h"
class DiagnosticNodeCustomComponent : public juce::Component,
                                      public juce::ListBoxModel {
 public:
  DiagnosticNodeCustomComponent(DiagnosticNode &node) : diagNode(node) {
    listBox.setModel(this);
    listBox.setRowHeight(20);
    listBox.setColour(juce::ListBox::backgroundColourId,
                      juce::Colour(0xff1a1a1a));
    addAndMakeVisible(listBox);

    startTimerHz(15);
  }

  void paint(juce::Graphics &g) override {
    g.fillAll(juce::Colour(0xff222222));
  }

  void resized() override { listBox.setBounds(getLocalBounds().reduced(2)); }

  // ListBoxModel
  int getNumRows() override { return (int)diagNode.getLatestSequence().size(); }

  void paintListBoxItem(int rowNumber, juce::Graphics &g, int width, int height,
                        bool rowIsSelected) override {
    juce::ignoreUnused(rowIsSelected);

    auto sequence = diagNode.getLatestSequence();
    if (rowNumber >= (int)sequence.size()) return;

    const auto &step = sequence[(size_t)rowNumber];

    g.setColour(juce::Colour(0xff2d2d2d));
    g.fillRoundedRectangle(2, 1, (float)width - 4, (float)height - 2, 3.0f);

    juce::String text;
    text << "[" << (rowNumber + 1) << "] ";

    if (step.empty()) {
      g.setColour(juce::Colours::grey);
      text << "REST";
    } else {
      g.setColour(juce::Colours::white);
      for (size_t i = 0; i < step.size(); ++i) {
        text << getNoteName(step[i].noteNumber);
        if (step[i].sourceNoteNumber != -1) {
          text << " (Src: " << step[i].sourceChannel << "."
               << getNoteName(step[i].sourceNoteNumber) << ")";
        }
        if (i < step.size() - 1) text << ", ";
      }
    }

    g.setFont(12.0f);
    g.drawText(text, 8, 0, width - 10, height,
               juce::Justification::centredLeft);
  }

 private:
  void timerCallback() {
    // Periodic refresh to catch sequence changes
    listBox.updateContent();
    listBox.repaint();
  }

  juce::String getNoteName(int noteNumber) {
    static const char *names[] = {"C",  "C#", "D",  "D#", "E",  "F",
                                  "F#", "G",  "G#", "A",  "A#", "B"};
    int octave = (noteNumber / 12) - 1;
    return juce::String(names[noteNumber % 12]) + juce::String(octave);
  }

  DiagnosticNode &diagNode;
  juce::ListBox listBox;

  struct Timer : public juce::Timer {
    std::function<void()> callback;
    void timerCallback() override {
      if (callback) callback();
    }
  };
  Timer refreshTimer;
  void startTimerHz(int hz) {
    refreshTimer.callback = [this] { timerCallback(); };
    refreshTimer.startTimerHz(hz);
  }
};

void DiagnosticNode::process() {
  auto it = inputSequences.find(0);
  if (it != inputSequences.end()) {
    latestSequence = it->second;
    outputSequences[0] = latestSequence;
  } else {
    latestSequence = NoteSequence();
    outputSequences[0] = latestSequence;
  }

  // Pass down
  auto connIt = connections.find(0);
  if (connIt != connections.end()) {
    for (const auto &connection : connIt->second) {
      connection.targetNode->setInputSequence(connection.targetInputPort,
                                              outputSequences[0]);
    }
  }
}

NodeLayout DiagnosticNode::getLayout() const {
  auto layout = LayoutParser::parseFromJSON(
      BinaryData::DiagnosticNode_json, BinaryData::DiagnosticNode_jsonSize);

  return layout;
}

std::unique_ptr<juce::Component> DiagnosticNode::createCustomComponent(
    const juce::String &name, juce::AudioProcessorValueTreeState *apvts) {
  juce::ignoreUnused(apvts);
  if (name == "ListBox")
    return std::make_unique<DiagnosticNodeCustomComponent>(*this);
  return nullptr;
}
