#include "SplitNode.h"

#include "../LayoutParser.h"
#include "BinaryData.h"
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

NodeLayout SplitNode::getLayout() const {
  auto layout = LayoutParser::parseFromJSON(BinaryData::SplitNode_json,
                                            BinaryData::SplitNode_jsonSize);

  // Bind runtime pointers by matching element labels
  for (auto &el : layout.elements) {
   if (el.label == "splitMode") {
      el.valueRef = const_cast<int *>(&splitMode);
    }
    else if (el.label == "splitPercent") {
      el.valueRef = const_cast<int *>(&splitPercent);
    }
  }

  return layout;
}
