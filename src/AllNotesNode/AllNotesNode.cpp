#include "AllNotesNode.h"

// --- AllNotesNode Impl

AllNotesNode::AllNotesNode() {}

NodeLayout AllNotesNode::getLayout() const {
  NodeLayout layout;
  layout.gridWidth = 1;
  layout.gridHeight = 1;

  UIElement label;
  label.type = UIElementType::Label;
  label.label = "Octave";
  label.gridBounds = {0, 0, 3, 1};
  layout.elements.push_back(label);

  UIElement combo;
  combo.type = UIElementType::ComboBox;
  combo.options = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10"};
  combo.valueRef = const_cast<int *>(&baseOctave);
  combo.gridBounds = {0, 1, 3, 1};
  layout.elements.push_back(combo);

  return layout;
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
