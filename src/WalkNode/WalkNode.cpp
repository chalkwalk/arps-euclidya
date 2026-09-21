#include "WalkNode.h"

#include <algorithm>
#include <cstdint>

#include <chalkwalk/seed/Derive.h>

#include "../LayoutParser.h"
#include "BinaryData.h"

// --- WalkNode Impl

NodeLayout WalkNode::getLayout() const {
  auto layout = LayoutParser::parseFromJSON(BinaryData::WalkNode_json,
                                            BinaryData::WalkNode_jsonSize);

  // Bind runtime pointers by matching element labels
  for (auto &el : layout.elements) {
    if (el.label == "walkLength") {
      el.valueRef = const_cast<int *>(&walkLength);
      el.macroParamRef = const_cast<MacroParam *>(&macroWalkLength);
    } else if (el.label == "walkSkewInt") {
      el.valueRef = const_cast<int *>(&walkSkewInt);
      el.macroParamRef = const_cast<MacroParam *>(&macroWalkSkew);
    }
  }

  return layout;
}

void WalkNode::saveNodeState(juce::XmlElement *xml) {
  if (xml != nullptr) {
    xml->setAttribute("walkLength", walkLength);
    xml->setAttribute("walkSkewInt", walkSkewInt);
    saveMacroBindings(xml);
  }
}

void WalkNode::loadNodeState(juce::XmlElement *xml) {
  if (xml != nullptr) {
    walkLength = xml->getIntAttribute("walkLength", 16);
    if (xml->hasAttribute("macroWalkLength")) {
      int m = xml->getIntAttribute("macroWalkLength", -1);
      if (m != -1)
        macroWalkLength.bindings.push_back({m, 1.0f});
    }
    if (xml->hasAttribute("macroWalkSkew")) {
      int m = xml->getIntAttribute("macroWalkSkew", -1);
      if (m != -1)
        macroWalkSkew.bindings.push_back({m, 1.0f});
    }
    loadMacroBindings(xml);
    if (xml->hasAttribute("walkSkewInt")) {
      walkSkewInt = xml->getIntAttribute("walkSkewInt", 0);
    } else {
      walkSkewInt = (int)(xml->getDoubleAttribute("walkSkew", 0.0) * 100.0);
    }
  }
}

void WalkNode::process() {
  int actualLength = resolveMacroInt(macroWalkLength, walkLength, 0, 64);
  actualLength = std::max(1, actualLength);

  float actualSkew =
      resolveMacroFloat(macroWalkSkew, walkSkewInt / 100.0f, -1.0f, 1.0f);

  auto it = inputSequences.find(0);
  if (it == inputSequences.end() || it->second.empty() || actualLength <= 0) {
    outputSequences[0] = NoteSequence();
  } else {
    NoteSequence steps = it->second;

    auto getMeanValue = [](const EventStep &step) {
      float sum = 0.0f;
      int count = 0;
      for (const auto &ev : step) {
        if (const auto *n = asNote(ev)) {
          sum += (float)n->noteNumber;
          ++count;
        }
      }
      return count > 0 ? sum / (float)count : 0.0f;
    };

    std::stable_sort(steps.begin(), steps.end(),
                     [&getMeanValue](const EventStep &a, const EventStep &b) {
                       return getMeanValue(a) < getMeanValue(b);
                     });

    // A seed from the sorted steps and the length, so the same input walks the
    // same way.
    //
    // ---- THIS USED TO BE STABLE ONLY ON ONE COMPILER ----
    //
    // It was `std::mt19937` seeded from a `size_t`, drawn through
    // `std::uniform_real_distribution<float>`. Both halves were wrong in the
    // same quiet way:
    //
    //   * `size_t` is eight bytes on a 64-bit build and four on a 32-bit one,
    //     so the seed itself differed before the engine started.
    //   * The mt19937 ENGINE is fully specified -- seeded 12345 it yields
    //     3992670690 everywhere. `uniform_real_distribution` IS NOT. The
    //     standard gives it no algorithm, so libstdc++ and libc++ hand back
    //     different numbers from the same engine state.
    //
    // So a patch saved on Linux walked differently when opened on macOS, and
    // nothing could catch it: each platform was perfectly consistent with
    // itself, and the comment above said "so it's stable", which it was --
    // per toolchain.
    //
    // `chalkwalk::seed` is integer mixing with a division by a power of two
    // and no `<random>` anywhere, which is the only way this is actually true.
    std::uint64_t seed = steps.size() + (std::uint64_t)actualLength;
    for (const auto &step : steps) {
      for (const auto &ev : step) {
        if (const auto *n = asNote(ev)) {
          seed = chalkwalk::seed::derive(seed, (std::uint64_t)n->noteNumber);
        }
      }
    }

    actualSkew = std::max(-1.0f, std::min(1.0f, actualSkew));
    float pv = std::max(0.0f, (1.0f / 3.0f) * (1.0f - actualSkew));
    float pn = std::max(0.0f, (1.0f / 3.0f) * (1.0f + actualSkew));
    float pc = std::max(0.0f, 1.0f - pv - pn);

    float total = pv + pc + pn;
    if (total > 0.0f) {
      pv /= total;
      pc /= total;
      pn /= total;
    } else {
      pv = pc = pn = 1.0f / 3.0f;
    }

    NoteSequence generatedSeq;

    if (!steps.empty()) {
      int currentIdx = (int)steps.size() / 2;

      for (int i = 0; i < actualLength; ++i) {
        generatedSeq.push_back(steps[(size_t)currentIdx]);

        // A fresh draw per step, derived from the step index rather than from
        // an advancing engine: the thousandth value costs the same as the
        // first and asking for it disturbs nothing else.
        const float r = (float)chalkwalk::seed::unitaryFor(seed, (std::uint64_t)i);
        if (r < pv) {
          currentIdx = (currentIdx - 1 + (int)steps.size()) % (int)steps.size();
        } else if (r >= pv + pc) {
          currentIdx = (currentIdx + 1) % (int)steps.size();
        }
      }
    }

    outputSequences[0] = generatedSeq;
  }

  auto connIt = connections.find(0);
  if (connIt != connections.end()) {
    for (const auto &connection : connIt->second) {
      connection.targetNode->setInputSequence(connection.targetInputPort,
                                              outputSequences[0]);
    }
  }
}
