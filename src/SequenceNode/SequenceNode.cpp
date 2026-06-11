#include "SequenceNode.h"

#include "../LayoutParser.h"
#include "BinaryData.h"

// In test builds the editor file is not compiled; the real implementation is
// in SequenceNodeEditor.cpp (plugin build only).
#ifdef ARPS_BUILD_TESTS
std::unique_ptr<juce::Component> SequenceNode::createCustomComponent(
    const juce::String &name, juce::AudioProcessorValueTreeState *apvts) {
  juce::ignoreUnused(name, apvts);
  return nullptr;
}
#endif

NodeLayout SequenceNode::getLayout() const {
  auto layout = LayoutParser::parseFromJSON(BinaryData::SequenceNode_json,
                                            BinaryData::SequenceNode_jsonSize);

  return layout;
}

void SequenceNode::saveNodeState(juce::XmlElement *xml) {
  if (xml) {
    xml->setAttribute("seqLength", seqLength);
    saveMacroBindings(xml);

    // Compress: only save active cells as "noteNum,step;" pairs
    juce::String activeStr;
    for (int n = 0; n < 128; ++n) {
      for (int c = 0; c < 16; ++c) {
        if (grid[n][c]) {
          activeStr += juce::String(n) + "," + juce::String(c) + ";";
        }
      }
    }
    xml->setAttribute("activeGrid", activeStr);
  }
}

void SequenceNode::loadNodeState(juce::XmlElement *xml) {
  if (xml) {
    seqLength = xml->getIntAttribute("seqLength", 8);
    if (xml->hasAttribute("macroSeqLength")) {
      int m = xml->getIntAttribute("macroSeqLength", -1);
      if (m != -1)
        macroSeqLength.bindings.push_back({m, 1.0f});
    }
    loadMacroBindings(xml);

    // Clear grid
    for (auto &n : grid) {
      for (int c = 0; c < 16; ++c) {
        n[c] = false;
      }
    }

    // Load sparse format
    const juce::String &activeStr = xml->getStringAttribute("activeGrid");
    if (activeStr.isNotEmpty()) {
      juce::StringArray pairs;
      pairs.addTokens(activeStr, ";", "");
      for (const auto &pair : pairs) {
        if (pair.isEmpty()) {
          continue;
        }
        int comma = pair.indexOfChar(',');
        if (comma > 0) {
          int n = pair.substring(0, comma).getIntValue();
          int c = pair.substring(comma + 1).getIntValue();
          if (n >= 0 && n < 128 && c >= 0 && c < 16) {
            grid[n][c] = true;
          }
        }
      }
    } else {
      // Legacy fallback: 8-row grid string
      const juce::String &gridStr = xml->getStringAttribute("grid");
      if (gridStr.length() == 128) {
        int baseNote = xml->getIntAttribute("baseNote", 60);
        int idx = 0;
        for (int r = 0; r < 8; ++r) {
          int noteNum = baseNote + (7 - r);
          for (int c = 0; c < 16; ++c) {
            grid[noteNum][c] = (gridStr[idx++] == '1');
          }
        }
      }
    }
  }
}

void SequenceNode::process() {
  int actualLen = resolveMacroInt(macroSeqLength, seqLength, 1, 16);
  actualLen = std::clamp(actualLen, 1, 16);

  NoteSequence outSeq;

  for (int c = 0; c < actualLen; ++c) {
    EventStep stepNotes;
    for (int n = 0; n < 128; ++n) {
      if (grid[n][c]) {
        HeldNote hn;
        hn.noteNumber = n;
        hn.channel = 1;
        hn.velocity = 0.8f;
        hn.sourceNoteNumber =
            n;  // For manual steps, the note is its own source
        hn.sourceChannel = 1;
        stepNotes.push_back(hn);
      }
    }
    outSeq.push_back(stepNotes);
  }

  outputSequences[0] = outSeq;

  auto conn = connections.find(0);
  if (conn != connections.end()) {
    for (const auto &connection : conn->second) {
      connection.targetNode->setInputSequence(connection.targetInputPort,
                                              outputSequences[0]);
    }
  }
}
