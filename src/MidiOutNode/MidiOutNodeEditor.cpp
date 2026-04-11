#include "../LayoutParser.h"
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

  auto bindElements = [self](std::vector<UIElement> &elements) {
    for (auto &el : elements) {
      if (el.label == "pSteps") {
        el.valueRef = &self->pSteps;
        el.macroParamRef = &self->macroPSteps;
      } else if (el.label == "pBeats") {
        el.valueRef = &self->pBeats;
        el.macroParamRef = &self->macroPBeats;
        el.dynamicMinRef = &self->ui_pBeatsMin;
        el.dynamicMaxRef = &self->ui_pBeatsMax;
      } else if (el.label == "pOffset") {
        el.valueRef = &self->pOffset;
        el.macroParamRef = &self->macroPOffset;
        el.dynamicMinRef = &self->ui_pOffsetMin;
        el.dynamicMaxRef = &self->ui_pOffsetMax;
      } else if (el.label == "rSteps") {
        el.valueRef = &self->rSteps;
        el.macroParamRef = &self->macroRSteps;
      } else if (el.label == "rBeats") {
        el.valueRef = &self->rBeats;
        el.macroParamRef = &self->macroRBeats;
        el.dynamicMinRef = &self->ui_rBeatsMin;
        el.dynamicMaxRef = &self->ui_rBeatsMax;
      } else if (el.label == "rOffset") {
        el.valueRef = &self->rOffset;
        el.macroParamRef = &self->macroROffset;
        el.dynamicMinRef = &self->ui_rOffsetMin;
        el.dynamicMaxRef = &self->ui_rOffsetMax;
      } else if (el.label == "pressureToVelocity") {
        el.floatValueRef = &self->pressureToVelocity;
        el.macroParamRef = &self->macroPressureToVelocity;
      } else if (el.label == "timbreToVelocity") {
        el.floatValueRef = &self->timbreToVelocity;
        el.macroParamRef = &self->macroTimbreToVelocity;
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
      } else if (el.label == "outputChannel") {
        el.valueRef = &self->outputChannel;
      } else if (el.label == "channelMode") {
        el.valueRef = &self->channelMode;
      } else if (el.label == "passExpressions") {
        el.valueRef = &self->ui_passExpressions;
      } else if (el.label == "humTiming") {
        el.floatValueRef = &self->humTiming;
        el.macroParamRef = &self->macroHumTiming;
      } else if (el.label == "humVelocity") {
        el.floatValueRef = &self->humVelocity;
        el.macroParamRef = &self->macroHumVelocity;
      } else if (el.label == "humGate") {
        el.floatValueRef = &self->humGate;
        el.macroParamRef = &self->macroHumGate;
      }
    }
  };

  bindElements(layout.elements);
  bindElements(layout.extendedElements);

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
