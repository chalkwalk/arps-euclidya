#include "AndNode.h"

#include "../LayoutParser.h"
#include "BinaryData.h"

namespace {

const NoteSequence kEmptySequence{};
const EventStep kEmptyStep{};

// Returns the subset of notes from stepA whose pitch also appears in stepB.
// The emitted note carries the higher velocity of the two, with the
// MpeCondition narrowed to the intersection of both predicates.
EventStep applyAnd(const EventStep &stepA, const EventStep &stepB) {
  EventStep out;
  for (const auto &evA : stepA) {
    const auto *noteA = asNote(evA);
    if (!noteA) continue;
    for (const auto &evB : stepB) {
      const auto *noteB = asNote(evB);
      if (!noteB || noteB->noteNumber != noteA->noteNumber) continue;
      const HeldNote *winner = (noteA->velocity >= noteB->velocity) ? noteA : noteB;
      const HeldNote *other  = (noteA->velocity >= noteB->velocity) ? noteB : noteA;
      HeldNote result = *winner;
      result.mpeCondition.intersectX(other->mpeCondition.xMin, other->mpeCondition.xMax);
      result.mpeCondition.intersectY(other->mpeCondition.yMin, other->mpeCondition.yMax);
      result.mpeCondition.intersectZ(other->mpeCondition.zMin, other->mpeCondition.zMax);
      out.push_back(result);
      break;
    }
  }
  return out;
}

}  // namespace

void AndNode::process() {
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
    out.push_back(applyAnd(a, b));
  }

  outputSequences[0] = std::move(out);

  auto connIt = connections.find(0);
  if (connIt != connections.end()) {
    for (const auto &c : connIt->second)
      c.targetNode->setInputSequence(c.targetInputPort, outputSequences[0]);
  }
}

NodeLayout AndNode::getLayout() const {
  auto layout = LayoutParser::parseFromJSON(BinaryData::AndNode_json,
                                            BinaryData::AndNode_jsonSize);
  auto *self = const_cast<AndNode *>(this);
  for (auto &el : layout.elements) {
    if (el.label == "lengthMode")
      el.valueRef = &self->lengthMode;
  }
  return layout;
}

void AndNode::saveNodeState(juce::XmlElement *xml) {
  if (xml)
    xml->setAttribute("lengthMode", lengthMode);
}

void AndNode::loadNodeState(juce::XmlElement *xml) {
  if (xml)
    lengthMode = xml->getIntAttribute("lengthMode", 0);
}
