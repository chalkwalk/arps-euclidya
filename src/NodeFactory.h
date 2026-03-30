#pragma once

#include <memory>
#include <string>

#include "AllNotesNode/AllNotesNode.h"
#include "ChordNNode/ChordNNode.h"
#include "ChordSplitNode/ChordSplitNode.h"
#include "ConcatenateNode/ConcatenateNode.h"
#include "ConvergeNode/ConvergeNode.h"
#include "DiagnosticNode/DiagnosticNode.h"
#include "DivergeNode/DivergeNode.h"
#include "FoldNode/FoldNode.h"
#include "GraphNode.h"
#include "MidiInNode/MidiInNode.h"
#include "MidiOutNode/MidiOutNode.h"
#include "MultiplyNode/MultiplyNode.h"
#include "OctaveStackNode/OctaveStackNode.h"
#include "OctaveTransposeNode/OctaveTransposeNode.h"
#include "QuantizerNode/QuantizerNode.h"
#include "ReverseNode/ReverseNode.h"
#include "RouteNode/RouteNode.h"
#include "SelectNode/SelectNode.h"
#include "SequenceNode/SequenceNode.h"
#include "SortNode/SortNode.h"
#include "SplitNode/SplitNode.h"
#include "SwitchNode/SwitchNode.h"
#include "TextNoteNode/TextNoteNode.h"
#include "TransposeNode/TransposeNode.h"
#include "UnfoldNode/UnfoldNode.h"
#include "UnzipNode/UnzipNode.h"
#include "WalkNode/WalkNode.h"
#include "ZipNode/ZipNode.h"

class NodeFactory {
 public:
  static std::shared_ptr<GraphNode> createNode(
      const std::string &type, NoteExpressionManager &midiCtx, ClockManager &clockCtx,
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
    if (type == "Note")
      return std::make_shared<TextNoteNode>(2, 1);
    if (type == "Note Large")
      return std::make_shared<TextNoteNode>(3, 2);
    return nullptr;
  }

  static std::vector<std::string> getAvailableNodeTypes() {
    return {"Midi In",
            "Sort",
            "Reverse",
            "Converge",
            "Diverge",
            "Diagnostic",
            "ChordN",
            "Walk",
            "Octave Stack",
            "Transpose",
            "Octave Transpose",
            "Fold",
            "Unfold",
            "Quantizer",
            "All Notes",
            "Split",
            "Concatenate",
            "Chord Split",
            "Multiply",
            "Zip",
            "Unzip",
            "Route",
            "Select",
            "Switch",
            "Sequence",
            "Midi Out",
            "Note",
            "Note Large"};
  }

  struct NodeMetadata {
    int gridW = 1, gridH = 1, numIn = 1, numOut = 1;
  };

  static NodeMetadata getPreviewMetadata(const std::string &type) {
    NodeMetadata m;
    if (type == "Note") {
      m.gridW = 2;
      m.gridH = 1;
      m.numIn = 0;
      m.numOut = 0;
    } else if (type == "Note Large") {
      m.gridW = 3;
      m.gridH = 2;
      m.numIn = 0;
      m.numOut = 0;
    } else if (type == "Midi In") {
      m.gridW = 1;
      m.gridH = 1;
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
               type == "Split" || type == "Walk" || type == "Unfold") {
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
