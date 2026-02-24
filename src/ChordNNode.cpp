#include "ChordNNode.h"
#include "MacroMappingMenu.h"
#include <algorithm>

class ChordNNodeEditor : public juce::Component {
public:
  ChordNNodeEditor(ChordNNode &node, juce::AudioProcessorValueTreeState &apvts)
      : chordNNode(node) {

    auto setupSlider = [this, &apvts](
                           CustomMacroSlider &slider, juce::Label &label,
                           int &nodeValueRef, int &nodeMacroRef,
                           std::unique_ptr<MacroAttachment> &attachment,
                           const juce::String &labelText, int min, int max) {
      slider.setRange(min, max, 1);
      slider.setValue(nodeValueRef, juce::dontSendNotification);
      slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
      slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
      addAndMakeVisible(slider);

      label.setText(labelText, juce::dontSendNotification);
      label.setJustificationType(juce::Justification::centred);
      addAndMakeVisible(label);

      slider.onValueChange = [this, &slider, &nodeValueRef]() {
        nodeValueRef = (int)slider.getValue();
        if (chordNNode.onNodeDirtied)
          chordNNode.onNodeDirtied();
      };

      auto updateSliderVisibility = [&slider](int macro) {
        if (macro == -1) {
          slider.setAlpha(1.0f);
          slider.setEnabled(true);
        } else {
          slider.setAlpha(0.5f);
          slider.setEnabled(false);
        }
      };

      updateSliderVisibility(nodeMacroRef);

      slider.onRightClick = [&slider, &nodeMacroRef, &attachment, &apvts,
                             updateSliderVisibility]() {
        MacroMappingMenu::showMenu(
            &slider, nodeMacroRef,
            [&nodeMacroRef, &attachment, &apvts, &slider,
             updateSliderVisibility](int macroIndex) {
              nodeMacroRef = macroIndex;
              if (macroIndex == -1) {
                attachment.reset();
                slider.setTooltip("");
              } else {
                attachment = std::make_unique<MacroAttachment>(
                    apvts, "macro_" + juce::String(macroIndex + 1), slider);
                slider.setTooltip("Mapped to Macro " +
                                  juce::String(macroIndex + 1));
              }
              updateSliderVisibility(macroIndex);
            });
      };

      // INIT
      if (nodeMacroRef != -1) {
        juce::String paramID = "macro_" + juce::String(nodeMacroRef + 1);
        attachment = std::make_unique<MacroAttachment>(apvts, paramID, slider);
      }
    };

    setupSlider(nValueSlider, nValueLabel, chordNNode.nValue,
                chordNNode.macroNValue, nValueAttachment, "N Value", 1, 16);

    setSize(400, 150);
  }

  void resized() override {
    auto bounds = getLocalBounds();
    int w = getWidth() / 3;
    auto b1 = bounds.removeFromLeft(w).removeFromTop(getHeight() / 2);

    auto bCopy = b1;
    nValueLabel.setBounds(bCopy.removeFromBottom(20));
    int size = std::min(bCopy.getWidth(), bCopy.getHeight());
    nValueSlider.setBounds(bCopy.withSizeKeepingCentre(size, size));
  }

private:
  ChordNNode &chordNNode;
  CustomMacroSlider nValueSlider;
  juce::Label nValueLabel;
  std::unique_ptr<MacroAttachment> nValueAttachment;
};

// --- ChordNNode Impl

ChordNNode::ChordNNode(std::array<std::atomic<float> *, 32> &inMacros)
    : macros(inMacros) {}

std::unique_ptr<juce::Component>
ChordNNode::createEditorComponent(juce::AudioProcessorValueTreeState &apvts) {
  return std::make_unique<ChordNNodeEditor>(*this, apvts);
}

void ChordNNode::saveNodeState(juce::XmlElement *xml) {
  if (xml != nullptr) {
    xml->setAttribute("nValue", nValue);
    xml->setAttribute("macroNValue", macroNValue);
  }
}

void ChordNNode::loadNodeState(juce::XmlElement *xml) {
  if (xml != nullptr) {
    nValue = xml->getIntAttribute("nValue", 2);
    macroNValue = xml->getIntAttribute("macroNValue", -1);
  }
}

void ChordNNode::process() {
  int actualNValue =
      macroNValue != -1 && macros[macroNValue] != nullptr
          ? 1 + (int)std::round(macros[macroNValue]->load() * 15.0f)
          : nValue;

  auto it = inputSequences.find(0);
  if (it == inputSequences.end() || it->second.empty() || actualNValue <= 0) {
    outputSequences[0] = NoteSequence();
  } else {
    // 1. Extract all unique notes from the incoming sequence into a pool
    std::set<int> uniquePitches;
    std::vector<HeldNote> uniqueNotes;

    for (const auto &step : it->second) {
      for (const auto &note : step) {
        if (uniquePitches.find(note.noteNumber) == uniquePitches.end()) {
          uniquePitches.insert(note.noteNumber);
          uniqueNotes.push_back(note);
        }
      }
    }

    // 2. Sort the unique notes ascending by pitch
    std::sort(uniqueNotes.begin(), uniqueNotes.end(),
              [](const HeldNote &a, const HeldNote &b) {
                return a.noteNumber < b.noteNumber;
              });

    size_t n = std::min((size_t)actualNValue, uniqueNotes.size());
    NoteSequence sortedSeq;

    // 3. Generate N-note combinations from the flat unique pool
    if (n > 0 && n <= uniqueNotes.size()) {
      auto cmb = [&](auto &self, std::vector<HeldNote> cur, size_t sI) -> void {
        if (cur.size() == n) {
          sortedSeq.push_back(cur);
          return;
        }
        if (sI >= uniqueNotes.size())
          return;
        for (size_t i = sI; i < uniqueNotes.size(); ++i) {
          if (cur.size() + (uniqueNotes.size() - i) < n)
            break;

          std::vector<HeldNote> nextCur = cur;
          nextCur.push_back(uniqueNotes[i]);

          self(self, nextCur, i + 1);
        }
      };
      cmb(cmb, std::vector<HeldNote>(), 0);
    }

    outputSequences[0] = sortedSeq;
  }

  auto connIt = connections.find(0);
  if (connIt != connections.end()) {
    for (const auto &connection : connIt->second) {
      connection.targetNode->setInputSequence(connection.targetInputPort,
                                              outputSequences[0]);
    }
  }
}
