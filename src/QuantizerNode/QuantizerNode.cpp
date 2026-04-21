#include "QuantizerNode.h"

#include <algorithm>
#include <vector>

#include "../LayoutParser.h"
#include "../Scales/ScaleEditorComponent.h"
#include "../Scales/ScaleLibrary.h"
#include "BinaryData.h"

// Migration table: maps old integer tonality index to scale name.
static const juce::StringArray kLegacyScaleNames = {
    "Lydian", "Ionian",     "Mixolydian",      "Dorian",
    "Aeolian", "Phrygian",  "Locrian",          "Major",
    "Minor",   "Harmonic Minor", "Melodic Minor", "Major Pentatonic",
    "Minor Pentatonic", "Whole Tone", "Diminished"};

QuantizerNode::QuantizerNode() {
  macroMode.name = "Quant Snap";
  macroRestOnDrop.name = "Quant Rests";
  addMacroParam(&macroRootNote);
  addMacroParam(&macroMode);
  addMacroParam(&macroRestOnDrop);
  refreshStepMask();
}

void QuantizerNode::refreshStepMask() {
  juce::String tuningName = (activeTuning != nullptr) ? activeTuning->name : "";
  const Scale *scale =
      ScaleLibrary::getInstance().findScale(selectedScaleName, tuningName);

  int steps = (activeTuning != nullptr && activeTuning->stepsPerOctave > 0)
                  ? activeTuning->stepsPerOctave
                  : 12;
  uint32_t bits = 0;
  if (scale != nullptr && !scale->stepMask.empty()) {
    steps = static_cast<int>(scale->stepMask.size());
    for (int i = 0; i < steps && i < 32; ++i) {
      if (scale->stepMask[(size_t)i]) {
        bits |= (1u << i);
      }
    }
  } else {
    // Fallback: all steps active (chromatic for this tuning)
    bits = (steps >= 32) ? ~0u : ((1u << steps) - 1u);
  }
  stepsPerOctave.store(steps);
  stepMaskBits.store(bits);
}

void QuantizerNode::parameterChanged() {
  GraphNode::parameterChanged();
  juce::String tuningName = (activeTuning != nullptr) ? activeTuning->name : "";
  auto names = ScaleLibrary::getInstance().getScaleNames(tuningName);
  if (scaleIndex >= 0 && scaleIndex < static_cast<int>(names.size())) {
    selectedScaleName = names[(size_t)scaleIndex];
  }
  refreshStepMask();
}

void QuantizerNode::setActiveTuning(const TuningTable *t) {
  activeTuning = t;
  // When tuning changes, update stepsPerOctave and try to keep selected scale.
  // If no scale exists for the new tuning, fall back to first available.
  juce::String tuningName = (t != nullptr) ? t->name : "";
  auto names = ScaleLibrary::getInstance().getScaleNames(tuningName);
  if (!names.empty()) {
    bool found = false;
    for (int i = 0; i < static_cast<int>(names.size()); ++i) {
      if (names[(size_t)i] == selectedScaleName) {
        scaleIndex = i;
        found = true;
        break;
      }
    }
    if (!found) {
      scaleIndex = 0;
      selectedScaleName = names[0];
    }
  } else {
    // No scales defined for this tuning — clear the name so the UI doesn't
    // show a stale 12-TET label. The step grid will be all-steps-on.
    scaleIndex = 0;
    selectedScaleName = "";
  }
  refreshStepMask();
}

NodeLayout QuantizerNode::getLayout() const {
  auto layout = LayoutParser::parseFromJSON(BinaryData::QuantizerNode_json,
                                            BinaryData::QuantizerNode_jsonSize);

  juce::String tuningName = (activeTuning != nullptr) ? activeTuning->name : "";
  auto scaleNames = ScaleLibrary::getInstance().getScaleNames(tuningName);

  // Sync scaleIndex to match selectedScaleName each time the layout is rebuilt.
  int idx = 0;
  for (int i = 0; i < static_cast<int>(scaleNames.size()); ++i) {
    if (scaleNames[(size_t)i] == selectedScaleName) {
      idx = i;
      break;
    }
  }
  scaleIndex = idx;

  for (auto &el : layout.elements) {
    if (el.label == "scaleName") {
      el.options = juce::StringArray(scaleNames.data(), (int)scaleNames.size());
      el.valueRef = &scaleIndex;
    } else if (el.label == "rootNote") {
      el.valueRef = const_cast<int *>(&rootNote);
      el.macroParamRef = const_cast<MacroParam *>(&macroRootNote);
    } else if (el.label == "mode") {
      el.valueRef = const_cast<int *>(&mode);
      el.macroParamRef = const_cast<MacroParam *>(&macroMode);
    } else if (el.label == "restOnDrop") {
      el.valueRef = const_cast<int *>(&restOnDrop);
      el.macroParamRef = const_cast<MacroParam *>(&macroRestOnDrop);
    }
  }

  return layout;
}

std::unique_ptr<juce::Component> QuantizerNode::createCustomComponent(
    const juce::String &name, juce::AudioProcessorValueTreeState *apvts) {
  juce::ignoreUnused(apvts);
  if (name == "ScaleEditor") {
    return std::make_unique<ScaleEditorComponent>(*this);
  }
  return nullptr;
}

void QuantizerNode::saveNodeState(juce::XmlElement *xml) {
  if (xml == nullptr) {
    return;
  }
  xml->setAttribute("selectedScaleName", selectedScaleName);
  xml->setAttribute("rootNote", rootNote);
  xml->setAttribute("mode", mode);
  xml->setAttribute("restOnDrop", restOnDrop);

  // Inline step mask for portability (patches shared without user-scale file)
  juce::String maskStr;
  uint32_t bits = stepMaskBits.load();
  int steps = stepsPerOctave.load();
  for (int i = 0; i < steps; ++i) {
    if (i > 0) { maskStr += ","; }
    maskStr += juce::String((bits >> i) & 1u);
  }
  xml->setAttribute("stepMask", maskStr);
  saveMacroBindings(xml);
}

void QuantizerNode::loadNodeState(juce::XmlElement *xml) {
  if (xml == nullptr) {
    return;
  }

  rootNote = xml->getIntAttribute("rootNote", 0);
  mode = xml->getIntAttribute("mode", 0);
  restOnDrop = xml->getIntAttribute("restOnDrop", 1);

  if (xml->hasAttribute("selectedScaleName")) {
    selectedScaleName = xml->getStringAttribute("selectedScaleName", "Ionian");
  } else if (xml->hasAttribute("tonality")) {
    // Migrate from old integer tonality format
    int t = xml->getIntAttribute("tonality", 1);
    if (t >= 0 && t < kLegacyScaleNames.size()) {
      selectedScaleName = kLegacyScaleNames[t];
    } else {
      selectedScaleName = "Ionian";
    }
  }

  // Try library lookup first; fall back to inline mask if name not found.
  juce::String tuningName = (activeTuning != nullptr) ? activeTuning->name : "";
  const Scale *scale =
      ScaleLibrary::getInstance().findScale(selectedScaleName, tuningName);

  if (scale == nullptr && xml->hasAttribute("stepMask")) {
    // Reconstruct from inline mask
    const auto &maskStr = xml->getStringAttribute("stepMask");
    auto tokens = juce::StringArray::fromTokens(maskStr, ",", "");
    std::vector<bool> mask;
    for (const auto &t : tokens) {
      mask.push_back(t.trim() == "1");
    }
    Scale fallback;
    fallback.name = selectedScaleName;
    fallback.tuningName = tuningName;
    fallback.stepMask = mask;
    int steps = static_cast<int>(mask.size());
    uint32_t bits = 0;
    for (int i = 0; i < steps && i < 32; ++i) {
      if (mask[(size_t)i]) { bits |= (1u << i); }
    }
    stepsPerOctave.store(steps > 0 ? steps : 12);
    stepMaskBits.store(bits);
  } else {
    refreshStepMask();
  }

  loadMacroBindings(xml);
}

void QuantizerNode::process() {
  const int effectiveMode = resolveMacroInt(macroMode, mode, 0, 1);
  const int effectiveRestOnDrop = resolveMacroInt(macroRestOnDrop, restOnDrop, 0, 1);
  const int steps = stepsPerOctave.load();
  const int effectiveRootNote = resolveMacroInt(macroRootNote, rootNote, 0,
                                                steps > 0 ? steps - 1 : 11);

  auto it = inputSequences.find(0);
  if (it == inputSequences.end() || it->second.empty()) {
    outputSequences[0] = NoteSequence();
  } else {
    const uint32_t mask = stepMaskBits.load();
    NoteSequence outSeq;

    for (const auto &step : it->second) {
      if (step.empty()) {
        outSeq.emplace_back();
        continue;
      }

      EventStep processedStep;

      for (const auto &ev : step) {
        const auto *originalNote = asNote(ev);
        if (originalNote == nullptr) {
          continue;
        }
        HeldNote quantizedNote = *originalNote;

        int pitchClass = (originalNote->noteNumber - effectiveRootNote) % steps;
        if (pitchClass < 0) {
          pitchClass += steps;
        }

        const bool inScale = (mask >> pitchClass) & 1u;

        if (effectiveMode == 0) {  // Filter
          if (inScale) {
            processedStep.push_back(quantizedNote);
          }
        } else {  // Snap
          if (inScale) {
            processedStep.push_back(quantizedNote);
          } else {
            int minDiff = 100;
            int bestAdjustment = 0;

            for (int s = 0; s < steps; ++s) {
              if (!((mask >> s) & 1u)) {
                continue;
              }
              int diff = s - pitchClass;
              if (diff > steps / 2) { diff -= steps; }
              if (diff < -(steps / 2)) { diff += steps; }

              if (std::abs(diff) < std::abs(minDiff)) {
                minDiff = diff;
                bestAdjustment = diff;
              } else if (std::abs(diff) == std::abs(minDiff) && diff > 0) {
                bestAdjustment = diff;  // Tie-break: prefer up
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
        std::sort(processedStep.begin(), processedStep.end(),
                  [](const SequenceEvent &a, const SequenceEvent &b) {
                    const auto *na = asNote(a);
                    const auto *nb = asNote(b);
                    return (na != nullptr ? na->noteNumber : 0) <
                           (nb != nullptr ? nb->noteNumber : 0);
                  });
        auto last = std::unique(
            processedStep.begin(), processedStep.end(),
            [](const SequenceEvent &a, const SequenceEvent &b) {
              const auto *na = asNote(a);
              const auto *nb = asNote(b);
              return (na != nullptr) && (nb != nullptr) &&
                     na->noteNumber == nb->noteNumber;
            });
        processedStep.erase(last, processedStep.end());
        outSeq.push_back(processedStep);
      } else if (effectiveMode == 0 && effectiveRestOnDrop == 1) {
        outSeq.emplace_back();
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
