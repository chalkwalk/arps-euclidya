#include "MpeFilterNode.h"

#include "../LayoutParser.h"
#include "BinaryData.h"

void MpeFilterNode::process() {
  auto it = inputSequences.find(0);
  if (it == inputSequences.end() || it->second.empty()) {
    outputSequences[0] = {};
    outputSequences[1] = {};
    return;
  }

  const auto &input = it->second;
  NoteSequence trueSeq(input.size());
  NoteSequence falseSeq(input.size());

  for (size_t s = 0; s < input.size(); ++s) {
    for (const auto &ev : input[s]) {
      const auto *note = asNote(ev);
      if (!note) { continue; }
      HeldNote trueNote = *note;
      HeldNote falseNote = *note;

      switch (resolveMacroInt(macroAxis, axis, 0, 2)) {
        case 0:  // Bend (X)
          trueNote.mpeCondition.intersectX(threshold, 1.0f);
          falseNote.mpeCondition.intersectX(0.0f, threshold);
          break;
        case 1:  // Timbre (Y)
          trueNote.mpeCondition.intersectY(threshold, 1.0f);
          falseNote.mpeCondition.intersectY(0.0f, threshold);
          break;
        case 2:  // Pressure (Z)
        default:
          trueNote.mpeCondition.intersectZ(threshold, 1.0f);
          falseNote.mpeCondition.intersectZ(0.0f, threshold);
          break;
      }

      trueSeq[s].push_back(trueNote);
      falseSeq[s].push_back(falseNote);
    }
  }

  outputSequences[0] = trueSeq;
  outputSequences[1] = falseSeq;
}

void MpeFilterNode::saveNodeState(juce::XmlElement *xml) {
  if (xml) {
    xml->setAttribute("axis", axis);
    xml->setAttribute("threshold", (double)threshold);
    saveMacroBindings(xml);
  }
}

void MpeFilterNode::loadNodeState(juce::XmlElement *xml) {
  if (xml) {
    axis = xml->getIntAttribute("axis", 0);
    threshold = (float)xml->getDoubleAttribute("threshold", 0.5);
    loadMacroBindings(xml);
  }
}

NodeLayout MpeFilterNode::getLayout() const {
  auto layout = LayoutParser::parseFromJSON(BinaryData::MpeFilterNode_json,
                                            BinaryData::MpeFilterNode_jsonSize);

  for (auto &el : layout.elements) {
    if (el.label == "axis") {
      el.valueRef = const_cast<int *>(&axis);
      el.macroParamRef = const_cast<MacroParam *>(&macroAxis);
    } else if (el.label == "threshold") {
      el.floatValueRef = const_cast<float *>(&threshold);
    }
  }

  return layout;
}
