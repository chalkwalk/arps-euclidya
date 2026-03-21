#include "UnfoldNode.h"
#include <algorithm>

class UnfoldNodeEditor : public juce::Component {
public:
  UnfoldNodeEditor(UnfoldNode &node, juce::AudioProcessorValueTreeState &)
      : unfoldNode(node) {

    toggle.setButtonText(node.ordering == 0 ? "Ascending" : "Descending");
    toggle.setToggleState(node.ordering != 0, juce::dontSendNotification);
    toggle.setClickingTogglesState(true);
    toggle.onClick = [this]() {
      unfoldNode.ordering = toggle.getToggleState() ? 1 : 0;
      toggle.setButtonText(unfoldNode.ordering == 0 ? "Ascending"
                                                    : "Descending");
      if (unfoldNode.onNodeDirtied)
        unfoldNode.onNodeDirtied();
    };
    addAndMakeVisible(toggle);

    setSize(160, 30);
  }

  void resized() override { toggle.setBounds(getLocalBounds().reduced(2)); }

private:
  UnfoldNode &unfoldNode;
  juce::ToggleButton toggle;
};

// --- UnfoldNode Impl

UnfoldNode::UnfoldNode() {}

std::unique_ptr<juce::Component>
UnfoldNode::createEditorComponent(juce::AudioProcessorValueTreeState &apvts) {
  return std::make_unique<UnfoldNodeEditor>(*this, apvts);
}

void UnfoldNode::saveNodeState(juce::XmlElement *xml) {
  if (xml != nullptr) {
    xml->setAttribute("ordering", ordering);
  }
}

void UnfoldNode::loadNodeState(juce::XmlElement *xml) {
  if (xml != nullptr) {
    ordering = xml->getIntAttribute("ordering", 0);
  }
}

void UnfoldNode::process() {
  auto it = inputSequences.find(0);
  if (it == inputSequences.end() || it->second.empty()) {
    outputSequences[0] = NoteSequence();
  } else {
    NoteSequence outSeq;

    for (const auto &step : it->second) {
      if (step.empty()) {
        outSeq.push_back({}); // preserve rest
        continue;
      }

      std::vector<HeldNote> currentStepNotes = step;

      if (ordering == 0) {
        // Ascending
        std::sort(currentStepNotes.begin(), currentStepNotes.end(),
                  [](const HeldNote &a, const HeldNote &b) {
                    return a.noteNumber < b.noteNumber;
                  });
      } else {
        // Descending
        std::sort(currentStepNotes.begin(), currentStepNotes.end(),
                  [](const HeldNote &a, const HeldNote &b) {
                    return a.noteNumber > b.noteNumber;
                  });
      }

      for (const auto &note : currentStepNotes) {
        outSeq.push_back({note});
      }
    }

    outputSequences[0] = outSeq;
  }

  auto connIt = connections.find(0);
  if (connIt != connections.end()) {
    for (const auto &connection : connIt->second) {
      connection.targetNode->setInputSequence(connection.targetInputPort,
                                              outputSequences[0]);
    }
  }
}
