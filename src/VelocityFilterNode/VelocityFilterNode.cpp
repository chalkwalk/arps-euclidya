#include "VelocityFilterNode.h"

#include "../LayoutParser.h"
#include "BinaryData.h"

void VelocityFilterNode::process() {
  auto it = inputSequences.find(0);
  if (it == inputSequences.end() || it->second.empty()) {
    outputSequences[0] = {};
    outputSequences[1] = {};
    return;
  }

  const auto &input = it->second;
  NoteSequence trueSeq(input.size());
  NoteSequence falseSeq(input.size());

  for (size_t s = 0; s < input.size(); ++s) {
    for (const HeldNote &note : input[s]) {
      if (note.velocity >= threshold) {
        trueSeq[s].push_back(note);
      } else {
        falseSeq[s].push_back(note);
      }
    }
    // Steps that end up empty are valid rests — leave them as-is.
  }

  outputSequences[0] = trueSeq;
  outputSequences[1] = falseSeq;
}

void VelocityFilterNode::saveNodeState(juce::XmlElement *xml) {
  if (xml) {
    xml->setAttribute("threshold", (double)threshold);
  }
}

void VelocityFilterNode::loadNodeState(juce::XmlElement *xml) {
  if (xml) {
    threshold = (float)xml->getDoubleAttribute("threshold", 0.5);
  }
}

NodeLayout VelocityFilterNode::getLayout() const {
  auto layout =
      LayoutParser::parseFromJSON(BinaryData::VelocityFilterNode_json,
                                  BinaryData::VelocityFilterNode_jsonSize);

  for (auto &el : layout.elements) {
    if (el.label == "threshold") {
      el.floatValueRef = const_cast<float *>(&threshold);
    }
  }

  return layout;
}
