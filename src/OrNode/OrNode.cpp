#include "OrNode.h"

#include "../LayoutParser.h"
#include "BinaryData.h"

namespace {

const NoteSequence kEmptySequence{};
const EventStep kEmptyStep{};

// Returns the union of notes from stepA and stepB by pitch.
// Pitches in both: highest-velocity note is emitted with the MpeCondition
// hulled across both predicates (or winner's condition if ranges are disjoint).
// Pitches in only one input pass through unchanged.
EventStep applyOr(const EventStep &stepA, const EventStep &stepB) {
  EventStep out;
  std::vector<int> pitchesInA;

  for (const auto &evA : stepA) {
    const auto *noteA = asNote(evA);
    if (!noteA) continue;
    pitchesInA.push_back(noteA->noteNumber);

    bool foundInB = false;
    for (const auto &evB : stepB) {
      const auto *noteB = asNote(evB);
      if (!noteB || noteB->noteNumber != noteA->noteNumber) continue;
      const HeldNote *winner = (noteA->velocity >= noteB->velocity) ? noteA : noteB;
      HeldNote result = *winner;
      MpeCondition hulled;
      if (MpeCondition::tryHull(noteA->mpeCondition, noteB->mpeCondition, hulled))
        result.mpeCondition = hulled;
      out.push_back(result);
      foundInB = true;
      break;
    }
    if (!foundInB)
      out.push_back(evA);
  }

  for (const auto &evB : stepB) {
    const auto *noteB = asNote(evB);
    if (!noteB) continue;
    bool inA = false;
    for (int p : pitchesInA)
      if (p == noteB->noteNumber) { inA = true; break; }
    if (!inA)
      out.push_back(evB);
  }

  return out;
}

}  // namespace

void OrNode::process() {
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
    out.push_back(applyOr(a, b));
  }

  outputSequences[0] = std::move(out);

  auto connIt = connections.find(0);
  if (connIt != connections.end()) {
    for (const auto &c : connIt->second)
      c.targetNode->setInputSequence(c.targetInputPort, outputSequences[0]);
  }
}

NodeLayout OrNode::getLayout() const {
  auto layout = LayoutParser::parseFromJSON(BinaryData::OrNode_json,
                                            BinaryData::OrNode_jsonSize);
  auto *self = const_cast<OrNode *>(this);
  for (auto &el : layout.elements) {
    if (el.label == "lengthMode")
      el.valueRef = &self->lengthMode;
  }
  return layout;
}

void OrNode::saveNodeState(juce::XmlElement *xml) {
  if (xml)
    xml->setAttribute("lengthMode", lengthMode);
}

void OrNode::loadNodeState(juce::XmlElement *xml) {
  if (xml)
    lengthMode = xml->getIntAttribute("lengthMode", 0);
}
