#pragma once

#include "GraphNode.h"
#include "MidiInNode.h"
#include "MidiOutNode.h"
#include "ReverseNode.h"
#include "SortNode.h"
#include <memory>
#include <string>

class NodeFactory {
public:
  static std::shared_ptr<GraphNode>
  createNode(const std::string &type, MidiHandler &midiCtx,
             ClockManager &clockCtx,
             std::array<std::atomic<float> *, 32> macros) {
    if (type == "Midi In")
      return std::make_shared<MidiInNode>(midiCtx);
    if (type == "Sort")
      return std::make_shared<SortNode>();
    if (type == "Reverse")
      return std::make_shared<ReverseNode>();
    if (type == "Midi Out")
      return std::make_shared<MidiOutNode>(midiCtx, clockCtx, macros);
    return nullptr;
  }

  static std::vector<std::string> getAvailableNodeTypes() {
    return {"Midi In", "Sort", "Reverse", "Midi Out"};
  }
};
