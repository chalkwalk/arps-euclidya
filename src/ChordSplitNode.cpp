#include "ChordSplitNode.h"
#include <algorithm>

void ChordSplitNode::process() {
  auto it = inputSequences.find(0);
  if (it == inputSequences.end() || it->second.empty()) {
    outputSequences[0] = {};
    outputSequences[1] = {};
    return;
  }

  const auto &seq = it->second;
  NoteSequence out0, out1;

  for (const auto &step : seq) {
    if (step.empty()) {
      // Rest: both outputs get a rest
      out0.push_back({});
      out1.push_back({});
      continue;
    }

    if (step.size() == 1) {
      // Single note: it goes to one output, the other gets a rest
      if (splitMode == 0) {
        // Top mode: the single note IS the highest, so it goes to out0 (Top)
        out0.push_back(step);
        out1.push_back({});
      } else {
        // Bottom mode: the single note IS the lowest, so it goes to out1
        // (Bottom)
        out0.push_back({});
        out1.push_back(step);
      }
      continue;
    }

    // Multiple notes: sort by pitch to find highest/lowest
    auto sorted = step;
    std::sort(sorted.begin(), sorted.end(),
              [](const HeldNote &a, const HeldNote &b) {
                return a.noteNumber < b.noteNumber;
              });

    if (splitMode == 0) {
      // Top mode: highest note → out0 (Top), rest → out1 (Bottom)
      std::vector<HeldNote> lower(sorted.begin(), sorted.end() - 1);
      std::vector<HeldNote> higher = {sorted.back()};
      out0.push_back(higher);
      out1.push_back(lower.empty() ? std::vector<HeldNote>{} : lower);
    } else {
      // Bottom mode: lowest note → out1 (Bottom), rest → out0 (Top)
      std::vector<HeldNote> lowest = {sorted.front()};
      std::vector<HeldNote> upper(sorted.begin() + 1, sorted.end());
      out0.push_back(upper.empty() ? std::vector<HeldNote>{} : upper);
      out1.push_back(lowest);
    }
  }

  outputSequences[0] = out0;
  outputSequences[1] = out1;
}

void ChordSplitNode::saveNodeState(juce::XmlElement *xml) {
  if (xml) {
    xml->setAttribute("splitMode", splitMode);
  }
}

void ChordSplitNode::loadNodeState(juce::XmlElement *xml) {
  if (xml) {
    splitMode = xml->getIntAttribute("splitMode", 0);
  }
}

// --- Editor ---
class ChordSplitNodeEditor : public juce::Component {
public:
  ChordSplitNodeEditor(ChordSplitNode &node) : chordSplitNode(node) {
    toggle.setButtonText(node.splitMode == 0 ? "TOP" : "BOTTOM");
    toggle.setToggleState(node.splitMode != 0, juce::dontSendNotification);
    toggle.setClickingTogglesState(true);
    toggle.onClick = [this]() {
      chordSplitNode.splitMode = toggle.getToggleState() ? 1 : 0;
      toggle.setButtonText(chordSplitNode.splitMode == 0 ? "TOP" : "BOTTOM");
      if (chordSplitNode.onNodeDirtied)
        chordSplitNode.onNodeDirtied();
    };
    addAndMakeVisible(toggle);

    setSize(160, 30);
  }

  void resized() override { toggle.setBounds(getLocalBounds().reduced(2)); }

private:
  ChordSplitNode &chordSplitNode;
  juce::ToggleButton toggle;
};

std::unique_ptr<juce::Component>
ChordSplitNode::createEditorComponent(juce::AudioProcessorValueTreeState &) {
  return std::make_unique<ChordSplitNodeEditor>(*this);
}
