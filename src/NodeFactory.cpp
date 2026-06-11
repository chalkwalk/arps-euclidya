#include "NodeFactory.h"

#include "AlgorithmicModulatorNode/AlgorithmicModulatorNode.h"
#include "AllNotesNode/AllNotesNode.h"
#include "AndNode/AndNode.h"
#include "ChordNNode/ChordNNode.h"
#include "ChordSplitNode/ChordSplitNode.h"
#include "ConcatenateNode/ConcatenateNode.h"
#include "ConvergeNode/ConvergeNode.h"
#include "DiagnosticNode/DiagnosticNode.h"
#include "DivergeNode/DivergeNode.h"
#include "FoldNode/FoldNode.h"
#include "InterleaveNode/InterleaveNode.h"
#include "MidiInNode/MidiInNode.h"
#include "MidiOutNode/MidiOutNode.h"
#include "MpeFilterNode/MpeFilterNode.h"
#include "MultiplyNode/MultiplyNode.h"
#include "OctaveStackNode/OctaveStackNode.h"
#include "OctaveTransposeNode/OctaveTransposeNode.h"
#include "OrNode/OrNode.h"
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
#include "VelocityFilterNode/VelocityFilterNode.h"
#include "WalkNode/WalkNode.h"
#include "XorNode/XorNode.h"
#include "ZipNode/ZipNode.h"

namespace {

using CreateFn = std::shared_ptr<GraphNode> (*)(NoteExpressionManager &,
                                                ClockManager &);

struct NodeTypeInfo {
  const char *name;
  CreateFn create;
  const char *category;
  std::vector<std::string> tags;
  NodeFactory::NodeMetadata preview;
};

// clang-format off
const std::vector<NodeTypeInfo> &registry() {
  static const std::vector<NodeTypeInfo> r = {
    // I/O
    {"Midi In",
     [](NoteExpressionManager &m, ClockManager &) -> std::shared_ptr<GraphNode> {
       return std::make_shared<MidiInNode>(m);
     },
     "I/O", {}, {1, 1, 0, 1}},
    {"Midi Out",
     [](NoteExpressionManager &m, ClockManager &c) -> std::shared_ptr<GraphNode> {
       return std::make_shared<MidiOutNode>(m, c);
     },
     "I/O", {}, {4, 3, 2, 0}},

    // Modulation
    {"CC Modulator",
     [](NoteExpressionManager &, ClockManager &) -> std::shared_ptr<GraphNode> {
       return std::make_shared<AlgorithmicModulatorNode>();
     },
     "Modulation", {"cc", "random"}, {3, 2, 0, 1}},

    // Generators
    {"All Notes",
     [](NoteExpressionManager &, ClockManager &) -> std::shared_ptr<GraphNode> {
       return std::make_shared<AllNotesNode>();
     },
     "Generators", {"harmony", "polyphony"}, {1, 1, 0, 1}},
    {"ChordN",
     [](NoteExpressionManager &, ClockManager &) -> std::shared_ptr<GraphNode> {
       return std::make_shared<ChordNNode>();
     },
     "Generators", {"harmony", "polyphony"}, {1, 1, 1, 1}},
    {"Sequence",
     [](NoteExpressionManager &, ClockManager &) -> std::shared_ptr<GraphNode> {
       return std::make_shared<SequenceNode>();
     },
     "Generators", {"melody", "rhythm"}, {4, 2, 0, 1}},
    {"Walk",
     [](NoteExpressionManager &, ClockManager &) -> std::shared_ptr<GraphNode> {
       return std::make_shared<WalkNode>();
     },
     "Generators", {"melody", "random"}, {2, 2, 1, 1}},

    // Pattern
    {"Converge",
     [](NoteExpressionManager &, ClockManager &) -> std::shared_ptr<GraphNode> {
       return std::make_shared<ConvergeNode>();
     },
     "Pattern", {}, {1, 1, 1, 1}},
    {"Diverge",
     [](NoteExpressionManager &, ClockManager &) -> std::shared_ptr<GraphNode> {
       return std::make_shared<DivergeNode>();
     },
     "Pattern", {}, {1, 1, 1, 1}},
    {"Fold",
     [](NoteExpressionManager &, ClockManager &) -> std::shared_ptr<GraphNode> {
       return std::make_shared<FoldNode>();
     },
     "Pattern", {"rhythm"}, {2, 2, 1, 1}},
    {"Multiply",
     [](NoteExpressionManager &, ClockManager &) -> std::shared_ptr<GraphNode> {
       return std::make_shared<MultiplyNode>();
     },
     "Pattern", {"rhythm"}, {1, 1, 1, 1}},
    {"Reverse",
     [](NoteExpressionManager &, ClockManager &) -> std::shared_ptr<GraphNode> {
       return std::make_shared<ReverseNode>();
     },
     "Pattern", {"rhythm", "melody"}, {1, 1, 1, 1}},
    {"Sort",
     [](NoteExpressionManager &, ClockManager &) -> std::shared_ptr<GraphNode> {
       return std::make_shared<SortNode>();
     },
     "Pattern", {"melody"}, {1, 1, 1, 1}},
    {"Unfold",
     [](NoteExpressionManager &, ClockManager &) -> std::shared_ptr<GraphNode> {
       return std::make_shared<UnfoldNode>();
     },
     "Pattern", {"rhythm", "melody"}, {2, 2, 1, 1}},

    // Combinatorial
    {"And",
     [](NoteExpressionManager &, ClockManager &) -> std::shared_ptr<GraphNode> {
       return std::make_shared<AndNode>();
     },
     "Combinatorial", {"logic", "harmony"}, {1, 1, 2, 1}},
    {"Chord Split",
     [](NoteExpressionManager &, ClockManager &) -> std::shared_ptr<GraphNode> {
       return std::make_shared<ChordSplitNode>();
     },
     "Combinatorial", {"harmony", "polyphony"}, {1, 1, 1, 2}},
    {"Concatenate",
     [](NoteExpressionManager &, ClockManager &) -> std::shared_ptr<GraphNode> {
       return std::make_shared<ConcatenateNode>();
     },
     "Combinatorial", {"melody"}, {1, 1, 2, 1}},
    {"Interleave",
     [](NoteExpressionManager &, ClockManager &) -> std::shared_ptr<GraphNode> {
       return std::make_shared<InterleaveNode>();
     },
     "Combinatorial", {"rhythm", "melody"}, {1, 1, 2, 1}},
    {"Or",
     [](NoteExpressionManager &, ClockManager &) -> std::shared_ptr<GraphNode> {
       return std::make_shared<OrNode>();
     },
     "Combinatorial", {"logic", "harmony"}, {1, 1, 2, 1}},
    {"Split",
     [](NoteExpressionManager &, ClockManager &) -> std::shared_ptr<GraphNode> {
       return std::make_shared<SplitNode>();
     },
     "Combinatorial", {}, {2, 2, 1, 2}},
    {"Unzip",
     [](NoteExpressionManager &, ClockManager &) -> std::shared_ptr<GraphNode> {
       return std::make_shared<UnzipNode>();
     },
     "Combinatorial", {}, {1, 1, 1, 2}},
    {"Xor",
     [](NoteExpressionManager &, ClockManager &) -> std::shared_ptr<GraphNode> {
       return std::make_shared<XorNode>();
     },
     "Combinatorial", {"logic", "harmony"}, {1, 1, 2, 1}},
    {"Zip",
     [](NoteExpressionManager &, ClockManager &) -> std::shared_ptr<GraphNode> {
       return std::make_shared<ZipNode>();
     },
     "Combinatorial", {"melody"}, {1, 1, 2, 1}},

    // Pitch & Range
    {"Octave Stack",
     [](NoteExpressionManager &, ClockManager &) -> std::shared_ptr<GraphNode> {
       return std::make_shared<OctaveStackNode>();
     },
     "Pitch & Range", {"harmony", "polyphony"}, {1, 1, 1, 1}},
    {"Octave Transpose",
     [](NoteExpressionManager &, ClockManager &) -> std::shared_ptr<GraphNode> {
       return std::make_shared<OctaveTransposeNode>();
     },
     "Pitch & Range", {"melody"}, {1, 1, 1, 1}},
    {"Quantizer",
     [](NoteExpressionManager &, ClockManager &) -> std::shared_ptr<GraphNode> {
       return std::make_shared<QuantizerNode>();
     },
     "Pitch & Range", {"melody", "tuning"}, {3, 2, 1, 1}},
    {"Transpose",
     [](NoteExpressionManager &, ClockManager &) -> std::shared_ptr<GraphNode> {
       return std::make_shared<TransposeNode>();
     },
     "Pitch & Range", {"melody"}, {1, 1, 1, 1}},

    // Routing
    {"Route",
     [](NoteExpressionManager &, ClockManager &) -> std::shared_ptr<GraphNode> {
       return std::make_shared<RouteNode>();
     },
     "Routing", {}, {1, 1, 1, 2}},
    {"Select",
     [](NoteExpressionManager &, ClockManager &) -> std::shared_ptr<GraphNode> {
       return std::make_shared<SelectNode>();
     },
     "Routing", {}, {1, 1, 2, 1}},
    {"Switch",
     [](NoteExpressionManager &, ClockManager &) -> std::shared_ptr<GraphNode> {
       return std::make_shared<SwitchNode>();
     },
     "Routing", {}, {1, 1, 1, 1}},

    // Filter
    {"MPE Filter",
     [](NoteExpressionManager &, ClockManager &) -> std::shared_ptr<GraphNode> {
       return std::make_shared<MpeFilterNode>();
     },
     "Filter", {"mpe"}, {2, 1, 1, 2}},
    {"Velocity Filter",
     [](NoteExpressionManager &, ClockManager &) -> std::shared_ptr<GraphNode> {
       return std::make_shared<VelocityFilterNode>();
     },
     "Filter", {"velocity"}, {1, 1, 1, 2}},

    // Utility
    {"Diagnostic",
     [](NoteExpressionManager &, ClockManager &) -> std::shared_ptr<GraphNode> {
       return std::make_shared<DiagnosticNode>();
     },
     "Utility", {}, {3, 2, 1, 1}},
    {"Note",
     [](NoteExpressionManager &, ClockManager &) -> std::shared_ptr<GraphNode> {
       return std::make_shared<TextNoteNode>(2, 1);
     },
     "Utility", {}, {2, 1, 0, 0}},
    {"Note Large",
     [](NoteExpressionManager &, ClockManager &) -> std::shared_ptr<GraphNode> {
       return std::make_shared<TextNoteNode>(3, 2);
     },
     "Utility", {}, {3, 2, 0, 0}},
  };
  return r;
}
// clang-format on

static const char *const kCategoryOrder[] = {
    "I/O",           "Modulation", "Generators", "Pattern", "Combinatorial",
    "Pitch & Range", "Routing",    "Filter",     "Utility"};

}  // namespace

std::shared_ptr<GraphNode> NodeFactory::createNode(
    const std::string &type, NoteExpressionManager &midiCtx,
    ClockManager &clockCtx) {
  for (const auto &entry : registry()) {
    if (type == entry.name)
      return entry.create(midiCtx, clockCtx);
  }
  return nullptr;
}

std::vector<std::string> NodeFactory::getAvailableNodeTypes() {
  std::vector<std::string> types;
  types.reserve(registry().size());
  for (const auto &entry : registry())
    types.emplace_back(entry.name);
  std::sort(types.begin(), types.end());
  return types;
}

std::vector<std::pair<std::string, std::vector<std::string>>>
NodeFactory::getNodeCategories() {
  std::vector<std::pair<std::string, std::vector<std::string>>> result;
  for (const char *cat : kCategoryOrder) {
    std::vector<std::string> names;
    for (const auto &entry : registry()) {
      if (std::string(entry.category) == cat)
        names.emplace_back(entry.name);
    }
    result.emplace_back(cat, std::move(names));
  }
  return result;
}

std::map<std::string, std::vector<std::string>> NodeFactory::getNodeTags() {
  std::map<std::string, std::vector<std::string>> tags;
  for (const auto &entry : registry()) {
    if (!entry.tags.empty())
      tags[entry.name] = entry.tags;
  }
  return tags;
}

NodeFactory::NodeMetadata NodeFactory::getPreviewMetadata(
    const std::string &type) {
  for (const auto &entry : registry()) {
    if (type == entry.name)
      return entry.preview;
  }
  return {};
}
