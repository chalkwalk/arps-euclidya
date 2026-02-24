#include "UnfoldNode.h"
#include <algorithm>

class UnfoldNodeEditor : public juce::Component {
public:
  UnfoldNodeEditor(UnfoldNode &node, juce::AudioProcessorValueTreeState &apvts)
      : unfoldNode(node) {

    orderingCombo.addItem("Ascending", 1);
    orderingCombo.addItem("Descending", 2);
    orderingCombo.setSelectedId(unfoldNode.ordering + 1,
                                juce::dontSendNotification);

    orderingCombo.onChange = [this]() {
      unfoldNode.ordering = orderingCombo.getSelectedId() - 1;
      if (unfoldNode.onNodeDirtied)
        unfoldNode.onNodeDirtied();
    };

    addAndMakeVisible(orderingCombo);

    orderingLabel.setText("Ordering", juce::dontSendNotification);
    orderingLabel.attachToComponent(&orderingCombo, false);
    addAndMakeVisible(orderingLabel);

    setSize(400, 150);
  }

  void resized() override {
    auto bounds = getLocalBounds().reduced(20);
    orderingCombo.setBounds(bounds.removeFromTop(30).withWidth(120).withX(20));
  }

private:
  UnfoldNode &unfoldNode;
  juce::ComboBox orderingCombo;
  juce::Label orderingLabel;
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
