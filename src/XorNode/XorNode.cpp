#include "XorNode.h"

#include "../LayoutParser.h"
#include "BinaryData.h"

namespace {

const NoteSequence kEmptySequence{};
const EventStep kEmptyStep{};

// Returns notes from stepA and stepB whose pitch appears in exactly one input.
// Surviving notes pass through with their original MpeCondition unchanged.
EventStep applyXor(const EventStep &stepA, const EventStep &stepB) {
  EventStep out;
  for (const auto &evA : stepA) {
    const auto *noteA = asNote(evA);
    if (!noteA) continue;
    bool inB = false;
    for (const auto &evB : stepB) {
      const auto *noteB = asNote(evB);
      if (noteB && noteB->noteNumber == noteA->noteNumber) { inB = true; break; }
    }
    if (!inB) out.push_back(evA);
  }
  for (const auto &evB : stepB) {
    const auto *noteB = asNote(evB);
    if (!noteB) continue;
    bool inA = false;
    for (const auto &evA : stepA) {
      const auto *noteA = asNote(evA);
      if (noteA && noteA->noteNumber == noteB->noteNumber) { inA = true; break; }
    }
    if (!inA) out.push_back(evB);
  }
  return out;
}

}  // namespace

void XorNode::process() {
  auto it0 = inputSequences.find(0);
  auto it1 = inputSequences.find(1);

  const NoteSequence &seq0 =
      (it0 != inputSequences.end()) ? it0->second : kEmptySequence;
  const NoteSequence &seq1 =
      (it1 != inputSequences.end()) ? it1->second : kEmptySequence;

  size_t len = (lengthMode == 1)
                   ? std::min(seq0.size(), seq1.size())
                   : std::max(seq0.size(), seq1.size());

  NoteSequence out;
  out.reserve(len);
  for (size_t i = 0; i < len; ++i) {
    const EventStep &a = (i < seq0.size()) ? seq0[i] : kEmptyStep;
    const EventStep &b = (i < seq1.size()) ? seq1[i] : kEmptyStep;
    out.push_back(applyXor(a, b));
  }

  outputSequences[0] = std::move(out);

  auto connIt = connections.find(0);
  if (connIt != connections.end()) {
    for (const auto &c : connIt->second)
      c.targetNode->setInputSequence(c.targetInputPort, outputSequences[0]);
  }
}

NodeLayout XorNode::getLayout() const {
  auto layout = LayoutParser::parseFromJSON(BinaryData::XorNode_json,
                                            BinaryData::XorNode_jsonSize);
  auto *self = const_cast<XorNode *>(this);
  for (auto &el : layout.elements) {
    if (el.label == "lengthMode")
      el.valueRef = &self->lengthMode;
  }
  return layout;
}

void XorNode::saveNodeState(juce::XmlElement *xml) {
  if (xml)
    xml->setAttribute("lengthMode", lengthMode);
}

void XorNode::loadNodeState(juce::XmlElement *xml) {
  if (xml)
    lengthMode = xml->getIntAttribute("lengthMode", 0);
}
