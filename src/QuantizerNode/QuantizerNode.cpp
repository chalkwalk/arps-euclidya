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

// --- QuantizerNode Impl

QuantizerNode::QuantizerNode() {}

NodeLayout QuantizerNode::getLayout() const {
  NodeLayout layout;
  layout.gridWidth = 2;
  layout.gridHeight = 2;

  UIElement tonLabel;
  tonLabel.type = UIElementType::Label;
  tonLabel.label = "Scale";
  tonLabel.gridBounds = {0, 0, 3, 1};
  layout.elements.push_back(tonLabel);

  UIElement tonalityBox;
  tonalityBox.type = UIElementType::ComboBox;
  tonalityBox.options = {
      "Lydian",           "Ionian",        "Mixolydian",
      "Dorian",           "Aeolian",       "Phrygian",
      "Locrian",          "Major",         "Minor",
      "Harmonic Minor",   "Melodic Minor", "Major Pentatonic",
      "Minor Pentatonic", "Whole Tone",    "Diminished"};
  tonalityBox.valueRef = const_cast<int *>(&tonality);
  tonalityBox.gridBounds = {3, 0, 4, 1};
  layout.elements.push_back(tonalityBox);

  UIElement rLabel;
  rLabel.type = UIElementType::Label;
  rLabel.label = "Root";
  rLabel.gridBounds = {0, 1, 3, 1};
  layout.elements.push_back(rLabel);

  UIElement rootBox;
  rootBox.type = UIElementType::ComboBox;
  rootBox.options = {"C",  "C#", "D",  "D#", "E",  "F",
                     "F#", "G",  "G#", "A",  "A#", "B"};
  rootBox.valueRef = const_cast<int *>(&rootNote);
  rootBox.gridBounds = {3, 1, 4, 1};
  layout.elements.push_back(rootBox);

  UIElement mLabel;
  mLabel.type = UIElementType::Label;
  mLabel.label = "Mode";
  mLabel.gridBounds = {0, 2, 3, 1};
  layout.elements.push_back(mLabel);

  UIElement modeToggle;
  modeToggle.type = UIElementType::Toggle;
  modeToggle.label = mode == 0 ? "Filter" : "Snap";
  modeToggle.valueRef = const_cast<int *>(&mode);
  modeToggle.gridBounds = {3, 2, 4, 1};
  layout.elements.push_back(modeToggle);

  return layout;
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
