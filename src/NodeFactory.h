#pragma once

#include "AllNotesNode.h"
#include "ChordNNode.h"
#include "ChordSplitNode.h"
#include "ConcatenateNode.h"
#include "ConvergeNode.h"
#include "DiagnosticNode.h"
#include "DivergeNode.h"
#include "FoldNode.h"
#include "GraphNode.h"
#include "MidiInNode.h"
#include "MidiOutNode.h"
#include "MultiplyNode.h"
#include "OctaveStackNode.h"
#include "OctaveTransposeNode.h"
#include "QuantizerNode.h"
#include "ReverseNode.h"
#include "RouteNode.h"
#include "SelectNode.h"
#include "SequenceNode.h"
#include "SortNode.h"
#include "SplitNode.h"
#include "SwitchNode.h"
#include "TransposeNode.h"
#include "UnfoldNode.h"
#include "UnzipNode.h"
#include "WalkNode.h"
#include "ZipNode.h"
#include <memory>
#include <string>

class NodeFactory {
public:
  static std::shared_ptr<GraphNode>
  createNode(const std::string &type, MidiHandler &midiCtx,
             ClockManager &clockCtx,
             std::array<std::atomic<float> *, 32> &macros) {
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
    if (type == "Diagnostic")
      return std::make_shared<DiagnosticNode>();
    if (type == "ChordN")
      return std::make_shared<ChordNNode>(macros);
    if (type == "Walk")
      return std::make_shared<WalkNode>(macros);
    if (type == "Octave Stack")
      return std::make_shared<OctaveStackNode>(macros);
    if (type == "Transpose")
      return std::make_shared<TransposeNode>(macros);
    if (type == "Octave Transpose")
      return std::make_shared<OctaveTransposeNode>(macros);
    if (type == "Fold")
      return std::make_shared<FoldNode>(macros);
    if (type == "Unfold")
      return std::make_shared<UnfoldNode>();
    if (type == "Quantizer")
      return std::make_shared<QuantizerNode>();
    if (type == "All Notes")
      return std::make_shared<AllNotesNode>();
    if (type == "Split")
      return std::make_shared<SplitNode>();
    if (type == "Concatenate")
      return std::make_shared<ConcatenateNode>();
    if (type == "Chord Split")
      return std::make_shared<ChordSplitNode>();
    if (type == "Multiply")
      return std::make_shared<MultiplyNode>(macros);
    if (type == "Zip")
      return std::make_shared<ZipNode>();
    if (type == "Unzip")
      return std::make_shared<UnzipNode>();
    if (type == "Sequence")
      return std::make_shared<SequenceNode>(macros);
    if (type == "Route")
      return std::make_shared<RouteNode>(macros);
    if (type == "Select")
      return std::make_shared<SelectNode>(macros);
    if (type == "Switch")
      return std::make_shared<SwitchNode>(macros);
    if (type == "Midi Out")
      return std::make_shared<MidiOutNode>(midiCtx, clockCtx, macros);
    return nullptr;
  }

  static std::vector<std::string> getAvailableNodeTypes() {
    return {"Midi In",      "Sort",        "Reverse",          "Converge",
            "Diverge",      "Diagnostic",  "ChordN",           "Walk",
            "Octave Stack", "Transpose",   "Octave Transpose", "Fold",
            "Unfold",       "Quantizer",   "All Notes",        "Split",
            "Concatenate",  "Chord Split", "Multiply",         "Zip",
            "Unzip",        "Route",       "Select",           "Switch",
            "Sequence",     "Midi Out"};
  }

  struct NodeMetadata {
    int gridW = 1, gridH = 1, numIn = 1, numOut = 1;
  };

  static NodeMetadata getPreviewMetadata(const std::string &type) {
    NodeMetadata m;
    if (type == "Midi In") {
      m.gridW = 2;
      m.gridH = 2;
      m.numIn = 0;
    } else if (type == "Midi Out") {
      m.gridW = 4;
      m.gridH = 3;
      m.numOut = 0;
    } else if (type == "Sequence") {
      m.gridW = 4;
      m.gridH = 2;
      m.numIn = 0;
    } else if (type == "Diagnostic" || type == "Fold" || type == "Quantizer" ||
               type == "Split" || type == "Walk") {
      m.gridW = 2;
      m.gridH = 2;
      if (type == "Split")
        m.numOut = 2;
    } else if (type == "Chord Split" || type == "Diverge" || type == "Unzip") {
      m.numOut = 2;
    } else if (type == "Converge" || type == "Zip" || type == "Concatenate") {
      m.numIn = 2;
    }
    return m;
  }
};
