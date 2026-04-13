#include "InterleaveNode.h"

#include "../LayoutParser.h"
#include "BinaryData.h"

void InterleaveNode::process() {
  auto it0 = inputSequences.find(0);
  auto it1 = inputSequences.find(1);

  static const NoteSequence kSingleRest{{}};
  const NoteSequence &seq0 =
      (it0 != inputSequences.end() && !it0->second.empty()) ? it0->second
                                                             : kSingleRest;
  const NoteSequence &seq1 =
      (it1 != inputSequences.end() && !it1->second.empty()) ? it1->second
                                                             : kSingleRest;

  const size_t maxLen = std::max(seq0.size(), seq1.size());
  NoteSequence out;
  out.reserve(maxLen * 2);

  for (size_t i = 0; i < maxLen; ++i) {
    out.push_back(i < seq0.size() ? seq0[i] : std::vector<HeldNote>{});
    out.push_back(i < seq1.size() ? seq1[i] : std::vector<HeldNote>{});
  }

  outputSequences[0] = std::move(out);
}

NodeLayout InterleaveNode::getLayout() const {
  return LayoutParser::parseFromJSON(BinaryData::InterleaveNode_json,
                                     BinaryData::InterleaveNode_jsonSize);
}
