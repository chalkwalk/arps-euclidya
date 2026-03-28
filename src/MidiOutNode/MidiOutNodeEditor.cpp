#include "../LayoutParser.h"
#include "../MacroMappingMenu.h"
#include "../SharedMacroUI.h"
#include "BinaryData.h"
#include "EuclideanVisualizer.h"
#include "MidiOutNode.h"

// --- Visualizer Component (the only custom component for MidiOutNode) ---
class MidiOutVisualizer : public juce::Component, private juce::Timer {
 public:
  MidiOutVisualizer(MidiOutNode &node) : midiOutNode(node) {
    addAndMakeVisible(visualizer);
    startTimerHz(30);
  }

  ~MidiOutVisualizer() override { stopTimer(); }

  void timerCallback() override {
    visualizer.update(midiOutNode.getPattern(), midiOutNode.getPatternIndex(),
                      midiOutNode.getRhythm(), midiOutNode.getRhythmIndex(),
                      midiOutNode.lastTickPlayedNote);
  }

  void resized() override { visualizer.setBounds(getLocalBounds()); }

 private:
  MidiOutNode &midiOutNode;
  EuclideanVisualizer visualizer;
};

// --- getLayout: load from JSON and bind all runtime pointers ---
NodeLayout MidiOutNode::getLayout() const {
  auto layout = LayoutParser::parseFromJSON(BinaryData::MidiOutNode_json,
                                            BinaryData::MidiOutNode_jsonSize);

  // Cast away const for pointer binding (NodeBlock reads/writes these)
  auto *self = const_cast<MidiOutNode *>(this);

  for (auto &el : layout.elements) {
    if (el.label == "pSteps") {
      el.valueRef = &self->pSteps;
      el.macroIndexRef = &self->macroPSteps;
    } else if (el.label == "pBeats") {
      el.valueRef = &self->pBeats;
      el.macroIndexRef = &self->macroPBeats;
      el.dynamicMinRef = &self->ui_pBeatsMin;
      el.dynamicMaxRef = &self->ui_pBeatsMax;
    } else if (el.label == "pOffset") {
      el.valueRef = &self->pOffset;
      el.macroIndexRef = &self->macroPOffset;
      el.dynamicMinRef = &self->ui_pOffsetMin;
      el.dynamicMaxRef = &self->ui_pOffsetMax;
    } else if (el.label == "rSteps") {
      el.valueRef = &self->rSteps;
      el.macroIndexRef = &self->macroRSteps;
    } else if (el.label == "rBeats") {
      el.valueRef = &self->rBeats;
      el.macroIndexRef = &self->macroRBeats;
      el.dynamicMinRef = &self->ui_rBeatsMin;
      el.dynamicMaxRef = &self->ui_rBeatsMax;
    } else if (el.label == "rOffset") {
      el.valueRef = &self->rOffset;
      el.macroIndexRef = &self->macroROffset;
      el.dynamicMinRef = &self->ui_rOffsetMin;
      el.dynamicMaxRef = &self->ui_rOffsetMax;
    } else if (el.label == "pressureToVelocity") {
      el.floatValueRef = &self->pressureToVelocity;
      el.macroIndexRef = &self->macroPressureToVelocity;
    } else if (el.label == "timbreToVelocity") {
      el.floatValueRef = &self->timbreToVelocity;
      el.macroIndexRef = &self->macroTimbreToVelocity;
    } else if (el.label == "clockDivisionIndex") {
      el.valueRef = &self->clockDivisionIndex;
    } else if (el.label == "syncMode") {
      el.valueRef = &self->ui_syncMode;
    } else if (el.label == "patternMode") {
      el.valueRef = &self->ui_patternMode;
    } else if (el.label == "triplet") {
      el.valueRef = &self->ui_triplet;
    } else if (el.label == "patternResetOnRelease") {
      el.valueRef = &self->ui_patternResetOnRelease;
    } else if (el.label == "rhythmResetOnRelease") {
      el.valueRef = &self->ui_rhythmResetOnRelease;
    }
  }

  return layout;
}

std::unique_ptr<juce::Component> MidiOutNode::createCustomComponent(
    const juce::String &name, juce::AudioProcessorValueTreeState *apvts) {
  juce::ignoreUnused(apvts);
  if (name == "Visualizer") {
    return std::make_unique<MidiOutVisualizer>(*this);
  }
  return nullptr;
}
