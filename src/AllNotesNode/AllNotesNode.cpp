#include "AllNotesNode.h"

#include "../LayoutParser.h"
#include "BinaryData.h"
// --- AllNotesNode Impl

AllNotesNode::AllNotesNode() {}

NodeLayout AllNotesNode::getLayout() const {
  auto layout = LayoutParser::parseFromJSON(BinaryData::AllNotesNode_json,
                                            BinaryData::AllNotesNode_jsonSize);

  // Bind runtime pointers by matching element labels
  for (auto &el : layout.elements) {
    if (el.label == "baseOctave") {
      el.valueRef = const_cast<int *>(&baseOctave);
    }
  }

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
      hn.channel = 1;     // Default
      hn.velocity = 100;  // Default
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
