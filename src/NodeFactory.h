#pragma once

#include "ChordNNode.h"
#include "ConvergeNode.h"
#include "DivergeNode.h"
#include "GraphNode.h"
#include "MidiInNode.h"
#include "MidiOutNode.h"
#include "OctaveStackNode.h"
#include "ReverseNode.h"
#include "SortNode.h"
#include "WalkNode.h"
#include <memory>
#include <string>

class NodeFactory {
public:
  static std::shared_ptr<GraphNode>
  createNode(const std::string &type, MidiHandler &midiCtx,
             ClockManager &clockCtx,
             std::array<std::atomic<float> *, 32> macros) {
    if (type == "Midi In")
      return std::make_shared<MidiInNode>(midiCtx, macros);
    if (type == "Sort")
      return std::make_shared<SortNode>();
    if (type == "Reverse")
      return std::make_shared<ReverseNode>();
    if (type == "Converge")
      return std::make_shared<ConvergeNode>();
    if (type == "Diverge")
      return std::make_shared<DivergeNode>();
    if (type == "ChordN")
      return std::make_shared<ChordNNode>(macros);
    if (type == "Walk")
      return std::make_shared<WalkNode>(macros);
    if (type == "Octave Stack")
      return std::make_shared<OctaveStackNode>(macros);
    if (type == "Midi Out")
      return std::make_shared<MidiOutNode>(midiCtx, clockCtx, macros);
    return nullptr;
  }

  static std::vector<std::string> getAvailableNodeTypes() {
    return {"Midi In", "Sort", "Reverse",      "Converge", "Diverge",
            "ChordN",  "Walk", "Octave Stack", "Midi Out"};
  }
};
