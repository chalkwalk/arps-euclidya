#include "QuantizerNode.h"
#include <algorithm>
#include <vector>

const std::vector<std::vector<int>> SCALES = {
    {0, 2, 4, 6, 7, 9, 11},   // 0: Lydian
    {0, 2, 4, 5, 7, 9, 11},   // 1: Ionian (Major)
    {0, 2, 4, 5, 7, 9, 10},   // 2: Mixolydian
    {0, 2, 3, 5, 7, 9, 10},   // 3: Dorian
    {0, 2, 3, 5, 7, 8, 10},   // 4: Aeolian (Natural Minor)
    {0, 1, 3, 5, 7, 8, 10},   // 5: Phrygian
    {0, 1, 3, 5, 6, 8, 10},   // 6: Locrian
    {0, 2, 4, 5, 7, 9, 11},   // 7: Major (Same as Ionian)
    {0, 2, 3, 5, 7, 8, 10},   // 8: Minor (Same as Aeolian)
    {0, 2, 3, 5, 7, 8, 11},   // 9: Harmonic Minor
    {0, 2, 3, 5, 7, 9, 11},   // 10: Melodic Minor
    {0, 2, 4, 7, 9},          // 11: Major Pentatonic
    {0, 3, 5, 7, 10},         // 12: Minor Pentatonic
    {0, 2, 4, 6, 8, 10},      // 13: Whole Tone
    {0, 1, 3, 4, 6, 7, 9, 10} // 14: Diminished (Half-Whole)
};

class QuantizerNodeEditor : public juce::Component {
public:
  QuantizerNodeEditor(QuantizerNode &node,
                      juce::AudioProcessorValueTreeState &apvts)
      : quantizerNode(node) {

    // Tonality
    tonalityCombo.addItemList(
        {"Lydian", "Ionian", "Mixolydian", "Dorian", "Aeolian", "Phrygian",
         "Locrian", "Major", "Minor", "Harmonic Minor", "Melodic Minor",
         "Major Pentatonic", "Minor Pentatonic", "Whole Tone", "Diminished"},
        1);
    tonalityCombo.setSelectedId(quantizerNode.tonality + 1,
                                juce::dontSendNotification);
    tonalityCombo.onChange = [this]() {
      quantizerNode.tonality = tonalityCombo.getSelectedId() - 1;
      if (quantizerNode.onNodeDirtied)
        quantizerNode.onNodeDirtied();
    };
    addAndMakeVisible(tonalityCombo);

    tonalityLabel.setText("Scale", juce::dontSendNotification);
    tonalityLabel.attachToComponent(&tonalityCombo, true);
    addAndMakeVisible(tonalityLabel);

    // Root Note
    rootCombo.addItemList(
        {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"}, 1);
    rootCombo.setSelectedId(quantizerNode.rootNote + 1,
                            juce::dontSendNotification);
    rootCombo.onChange = [this]() {
      quantizerNode.rootNote = rootCombo.getSelectedId() - 1;
      if (quantizerNode.onNodeDirtied)
        quantizerNode.onNodeDirtied();
    };
    addAndMakeVisible(rootCombo);

    rootLabel.setText("Root", juce::dontSendNotification);
    rootLabel.attachToComponent(&rootCombo, true);
    addAndMakeVisible(rootLabel);

    // Mode
    modeToggle.setButtonText(quantizerNode.mode == 0 ? "Filter" : "Snap");
    modeToggle.setToggleState(quantizerNode.mode != 0,
                              juce::dontSendNotification);
    modeToggle.setClickingTogglesState(true);
    modeToggle.onClick = [this]() {
      quantizerNode.mode = modeToggle.getToggleState() ? 1 : 0;
      modeToggle.setButtonText(quantizerNode.mode == 0 ? "Filter" : "Snap");
      if (quantizerNode.onNodeDirtied)
        quantizerNode.onNodeDirtied();
    };
    addAndMakeVisible(modeToggle);

    modeLabel.setText("Mode", juce::dontSendNotification);
    modeLabel.attachToComponent(&modeToggle, true);
    addAndMakeVisible(modeLabel);

    setSize(400, 150);
  }

  void resized() override {
    auto bounds = getLocalBounds().reduced(10);
    int h = bounds.getHeight() / 3;
    int labelWidth = 60;

    auto row1 = bounds.removeFromTop(h).reduced(2);
    tonalityCombo.setBounds(row1.withTrimmedLeft(labelWidth));

    auto row2 = bounds.removeFromTop(h).reduced(2);
    rootCombo.setBounds(row2.withTrimmedLeft(labelWidth));

    auto row3 = bounds.removeFromTop(h).reduced(2);
    modeToggle.setBounds(row3.withTrimmedLeft(labelWidth));
  }

private:
  QuantizerNode &quantizerNode;
  juce::ComboBox tonalityCombo;
  juce::Label tonalityLabel;
  juce::ComboBox rootCombo;
  juce::Label rootLabel;
  juce::ToggleButton modeToggle;
  juce::Label modeLabel;
};

// --- QuantizerNode Impl

QuantizerNode::QuantizerNode() {}

std::unique_ptr<juce::Component> QuantizerNode::createEditorComponent(
    juce::AudioProcessorValueTreeState &apvts) {
  return std::make_unique<QuantizerNodeEditor>(*this, apvts);
}

void QuantizerNode::saveNodeState(juce::XmlElement *xml) {
  if (xml != nullptr) {
    xml->setAttribute("tonality", tonality);
    xml->setAttribute("rootNote", rootNote);
    xml->setAttribute("mode", mode);
  }
}

void QuantizerNode::loadNodeState(juce::XmlElement *xml) {
  if (xml != nullptr) {
    tonality = xml->getIntAttribute("tonality", 0);
    rootNote = xml->getIntAttribute("rootNote", 0);
    mode = xml->getIntAttribute("mode", 0);
  }
}

void QuantizerNode::process() {
  auto it = inputSequences.find(0);
  if (it == inputSequences.end() || it->second.empty()) {
    outputSequences[0] = NoteSequence();
  } else {
    NoteSequence outSeq;
    const auto &scalePattern = SCALES[tonality];

    for (const auto &step : it->second) {
      if (step.empty()) {
        outSeq.push_back({});
        continue;
      }

      std::vector<HeldNote> processedStep;

      for (const auto &originalNote : step) {
        HeldNote quantizedNote = originalNote;

        // Find interval relative to root
        int pitchClass = (originalNote.noteNumber - rootNote) % 12;
        if (pitchClass < 0)
          pitchClass += 12; // Handle negative modulo

        bool inScale = std::find(scalePattern.begin(), scalePattern.end(),
                                 pitchClass) != scalePattern.end();

        if (mode == 0) { // Filter
          if (inScale) {
            processedStep.push_back(quantizedNote);
          }
        } else { // Snap
          if (inScale) {
            processedStep.push_back(quantizedNote);
          } else {
            int minDiff = 100;
            int bestAdjustment = 0;

            for (int scalePitch : scalePattern) {
              int diff = scalePitch - pitchClass;

              if (diff > 6)
                diff -= 12;
              if (diff < -6)
                diff += 12;

              if (std::abs(diff) < std::abs(minDiff)) {
                minDiff = diff;
                bestAdjustment = diff;
              } else if (std::abs(diff) == std::abs(minDiff) && diff > 0) {
                // Tie breaker: prefer pushing up
                bestAdjustment = diff;
              }
            }

            quantizedNote.noteNumber += bestAdjustment;

            if (quantizedNote.noteNumber >= 0 &&
                quantizedNote.noteNumber <= 127) {
              processedStep.push_back(quantizedNote);
            }
          }
        }
      }

      if (!processedStep.empty()) {
        // Dedup notes in chord to avoid identical snapped pitches
        std::sort(processedStep.begin(), processedStep.end(),
                  [](const HeldNote &a, const HeldNote &b) {
                    return a.noteNumber < b.noteNumber;
                  });

        auto last = std::unique(processedStep.begin(), processedStep.end(),
                                [](const HeldNote &a, const HeldNote &b) {
                                  return a.noteNumber == b.noteNumber;
                                });
        processedStep.erase(last, processedStep.end());
        outSeq.push_back(processedStep);
      } else if (mode == 0) {
        outSeq.push_back({}); // Keep length if entirely filtered out
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
