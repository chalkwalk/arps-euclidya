#include "AllNotesNode.h"

class AllNotesNodeEditor : public juce::Component {
public:
  AllNotesNodeEditor(AllNotesNode &node,
                     juce::AudioProcessorValueTreeState &apvts)
      : allNotesNode(node) {

    octaveCombo.addItemList(
        {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10"}, 1);
    octaveCombo.setSelectedId(allNotesNode.baseOctave + 1,
                              juce::dontSendNotification);

    octaveCombo.onChange = [this]() {
      allNotesNode.baseOctave = octaveCombo.getSelectedId() - 1;
    };

    addAndMakeVisible(octaveCombo);

    octaveLabel.setText("Base Octave", juce::dontSendNotification);
    octaveLabel.attachToComponent(&octaveCombo, false);
    addAndMakeVisible(octaveLabel);

    setSize(400, 150);
  }

  void resized() override {
    auto bounds = getLocalBounds().reduced(20);
    octaveCombo.setBounds(bounds.removeFromTop(30).withWidth(120).withX(20));
  }

private:
  AllNotesNode &allNotesNode;
  juce::ComboBox octaveCombo;
  juce::Label octaveLabel;
};

// --- AllNotesNode Impl

AllNotesNode::AllNotesNode() {}

std::unique_ptr<juce::Component>
AllNotesNode::createEditorComponent(juce::AudioProcessorValueTreeState &apvts) {
  return std::make_unique<AllNotesNodeEditor>(*this, apvts);
}

void AllNotesNode::saveNodeState(juce::XmlElement *xml) {
  if (xml != nullptr) {
    xml->setAttribute("baseOctave", baseOctave);
  }
}

void AllNotesNode::loadNodeState(juce::XmlElement *xml) {
  if (xml != nullptr) {
    baseOctave = xml->getIntAttribute("baseOctave", 3);
  }
}

void AllNotesNode::process() {
  NoteSequence outSeq;
  int baseNoteNumber = baseOctave * 12;

  // Generate 12 steps, one for each chromatic note C -> B
  for (int i = 0; i < 12; ++i) {
    int currentNote = baseNoteNumber + i;
    if (currentNote <= 127) {
      HeldNote hn;
      hn.noteNumber = currentNote;
      hn.channel = 1;    // Default
      hn.velocity = 100; // Default
      outSeq.push_back({hn});
    }
  }

  outputSequences[0] = outSeq;

  auto connIt = connections.find(0);
  if (connIt != connections.end()) {
    for (const auto &connection : connIt->second) {
      connection.targetNode->setInputSequence(connection.targetInputPort,
                                              outputSequences[0]);
    }
  }
}
