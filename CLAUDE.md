# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**Arps Euclidya** is a modular, node-based MIDI effect plugin built with C++17, JUCE, and CMake. It routes MIDI through a directed acyclic graph (DAG) of 32 processing nodes to create complex rhythmic and melodic patterns, with full MPE and microtonality support. The project targets electronic musicians and is authored by ChalkWalk; external contributors are welcome.

The plugin produces **VST3**, **CLAP**, and **Standalone** binaries on Linux and Windows, and additionally **AU** on macOS. CLAP is the primary supported format. Development and manual testing happen on Linux (Bitwig Studio); the CI matrix also builds on macOS and Windows, but those platforms are not manually tested by the author — cross-platform validation from contributors is actively wanted.

## Build Commands

```bash
# First time: initialize submodules
git submodule update --init --recursive

mkdir build && cd build

# Configure — Clang is required on Linux (matches CI and strict warning flags)
cmake -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
      -DCMAKE_AR=llvm-ar -DCMAKE_RANLIB=llvm-ranlib ..

# macOS: omit the AR/RANLIB flags
# Windows: plain `cmake ..` (MSVC)

cmake --build . -j $(nproc)
```

Debug artifacts: `build/src/ArpsEuclidya_artefacts/Debug/`
Release artifacts (CI): `build/src/ArpsEuclidya_artefacts/Release/`

**Linux symlink install:**
```bash
ln -s $(pwd)/src/ArpsEuclidya_artefacts/Debug/VST3/"Arps Euclidya.vst3" ~/.vst3/
ln -s $(pwd)/src/ArpsEuclidya_artefacts/Debug/CLAP/"Arps Euclidya.clap" ~/.clap/
```

## Linting and Formatting

```bash
clang-format -i src/**/*.cpp src/**/*.h   # format in-place
clang-tidy src/*.cpp -- -std=c++17        # lint
```

Both `.clang-format` and `.clang-tidy` configs are at the repo root. All PRs must pass both. The Clang build enforces `-Wall -Wextra -Wpedantic -Wsign-conversion -Wfloat-conversion -Wshadow -Werror`.

## Testing

Configure with `-DARPS_BUILD_TESTS=ON` to build the Catch2 unit-test binary:

```bash
cmake -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
      -DCMAKE_AR=llvm-ar -DCMAKE_RANLIB=llvm-ranlib \
      -DARPS_BUILD_TESTS=ON -B build
cmake --build build -j $(nproc)
ctest --test-dir build --output-on-failure
```

Manual verification is also required:
1. Build succeeds with zero warnings under Clang strict flags.
2. Run the Standalone binary and exercise the changed area.
3. Load in Bitwig via CLAP and verify behavior under a running transport.
4. For cross-platform or VST3 changes, flag this in the PR for contributor testing.

## Architecture

### Data Model (`src/DataModel.h`)

The fundamental unit flowing between nodes is `NoteSequence` (aliased to `EventSequence`):

```
EventSequence  = std::vector<EventStep>
EventStep      = std::vector<SequenceEvent>          // one time step; empty = rest
SequenceEvent  = std::variant<HeldNote, CCEvent>     // note or CC value
NoteSequence   = EventSequence                       // legacy alias, still used everywhere
```

Access events with the free-function helpers — never use `std::get` directly:
```cpp
const HeldNote* n = asNote(event);   // nullptr if it's a CC event
const CCEvent*  c = asCC(event);     // nullptr if it's a note event
```

**`HeldNote`** carries: pitch, channel, velocity, MPE axes (mpeX = pitch bend, mpeY = CC74/timbre, mpeZ = pressure), source-tracking fields, note ID (CLAP), and a `MpeCondition` predicate.

**`MpeCondition`** is a per-note playback-time predicate holding `[min,max]` ranges for each MPE axis (X/Y/Z). It defaults to full passthrough. `MpeFilterNode` narrows one axis per instance; chaining multiple filter nodes narrows multiple axes. `MidiOutNode::generateOutput()` evaluates the predicate at tick time — notes whose condition fails are suppressed. Use `intersectX/Y/Z()` to narrow in-place; `tryHull()` merges two conditions when routing back together.

**`CCEvent`** carries: `ccNumber` (0–127), `value` (normalised 0..1), `channel`. The value is converted to 0–127 only at MIDI output.

### Port Types (`src/GraphNode.h`)

Every port on every node has a `PortType`:

| Enum | Meaning | Cable colour |
|------|---------|--------------|
| `PortType::Notes` | `HeldNote` events only | Blue/teal |
| `PortType::CC` | `CCEvent` events only | Violet |
| `PortType::Agnostic` | Accepts/emits either | Grey |

Nodes declare port types by overriding `getInputPortType(int)` / `getOutputPortType(int)`. The canvas enforces type compatibility when the user draws cables. A mismatched connection is rejected at the UI layer. Nodes that only handle notes can ignore port type methods — the default is `Notes`.

`getDeclaredCCNumber()` (default −1) lets CC generator nodes advertise their CC# so `MidiOutNode` can pre-populate its CC registry.

### Graph Execution (`src/GraphEngine.h/cpp`)

`GraphEngine` owns all nodes and connections. On each audio callback it:
1. Runs a **topological sort** (Kahn's algorithm) to determine processing order.
2. Calls `process()` on each node in order, passing upstream outputs as inputs.
3. Enforces DAG constraints (no cycles allowed).

The engine also distributes the 32 macro `std::atomic<float>*` pointers and the `macroBipolarMask` to every node after graph changes.

### Node Base Class (`src/GraphNode.h`)

`GraphNode` is the abstract base for all module types. Key responsibilities:

- Declares I/O ports (`EventSequence` inputs/outputs).
- Stores grid position (`gridX`, `gridY`) and bypass state.
- Implements a dirty-flag pattern so nodes only recompute when upstream data changes.
- Holds `std::array<std::atomic<float>*, 32> macros` — set by `GraphEngine`, read atomically in `process()`.
- Holds `std::atomic<uint32_t> macroBipolarMask` — bitmask where bit *i* = 1 means macro *i* is bipolar.
- Provides `resolveMacroInt` / `resolveMacroFloat` / `resolveMacroOffset` helpers for use in `process()`.
- Carries `std::function<void()> onMappingChanged` — set by `PluginProcessor` to call `updateMacroNames()` whenever bindings change.

Subclasses must implement `getName()` and `process()`.

### Macro System

The macro system maps up to 32 global DAW-automatable parameters to internal node controls with signed, additive intensity. Two separate types are involved:

- **`MacroParameter`** (`src/MacroParameter.h`): a `juce::AudioProcessorParameter` subclass, one per macro slot, exposed to the DAW via APVTS. Holds the knob value `[0,1]` and the display name (`mappingName`).
- **`MacroParam`** (`src/GraphNode.h`): a lightweight struct `{ juce::String name; std::vector<MacroBinding> bindings; }` that lives on each node. `MacroBinding` is `{ int macroIndex; float intensity; }`. This is the node-side record of what macros drive this parameter.

#### Registration

Every node with macro-bindable parameters declares `MacroParam` members and registers them by calling `addMacroParam(&myParam)` in its constructor. `GraphNode::getMacroParams()` (non-virtual) returns the registered list. **Never add or remove params after construction** — registration happens once and the vector is stable.

#### Resolution in `process()`

Nodes call `resolveMacroFloat(param, localVal, min, max)` or `resolveMacroInt(param, localVal, min, max)` to compute the effective value. These iterate all bindings on the `MacroParam` and additively apply each macro's contribution:

```
contribution = isBipolar ? (macroVal - 0.5) × 2 × intensity × range
                         : macroVal × intensity × range
effective = clamp(localVal + Σ contributions, min, max)
```

`resolveMacroOffset(param, localVal, maxVal)` is a convenience wrapper for bipolar offsets (range `[-maxVal, maxVal]`).

#### UI Pipeline

```
PluginEditor::selectedMacro (int, -1 = none)
  └─ GraphCanvas::selectedMacroPtr (int*)
       └─ NodeBlock::selectedMacroPtr (int*)
            └─ CustomMacroSlider::selectedMacroPtr
               CustomMacroButton::selectedMacroPtr
```

`CustomMacroSlider` / `CustomMacroButton` (`src/SharedMacroUI.h`) handle shift+drag/shift+click to create and adjust bindings. Each carries a `macroParamRef` pointer to the node's `MacroParam` and modifies `bindings` directly. After a binding change they call `onMappingChanged` (→ `updateMacroNames()`) and `onBindingChanged` (→ deferred `rebuild()`).

`NodeBlock::paintOverChildren` (`src/NodeBlock.cpp`) draws all macro visuals: colored intensity arcs (knobs), colored border rings (buttons), the effective-value indicator, "bindable" glow when a macro is selected, and the palette-hover white ring. It uses `sliderMacroInfos` / `buttonMacroInfos` vectors populated during `NodeBlock` construction to know which controls are macro-aware.

`src/MacroColours.h` provides `getMacroColour(int macroIndex)` — 32 distinct HSL hues used consistently across all macro visuals.

### Microtonality (`src/Tuning/`)

Microtonal output is implemented via MPE pitch-bend offsets applied per-note at output time in `MidiOutNode`. The `src/Tuning/` subsystem provides:

| File | Role |
|------|------|
| `TuningTable.h` | 128-entry cents-deviation array + SCL/KBM file references |
| `ScalaParser.h/cpp` | Parses `.scl` (Scala scale) and `.kbm` (keyboard mapping) files |
| `TuningLibrary.h/cpp` | Loads and manages multiple `TuningTable`s from disk |
| `TuningPanel.h/cpp` | UI panel for browsing and selecting tunings |

Tuning state is per-patch, serialised into the `.euclidya` XML. When a `TuningTable` is active, `MidiOutNode` adds `centsDeviation[noteNumber]` as an MPE pitch-bend expression message alongside the note-on.

### Adding a New Node

Each node lives in its own subdirectory:
```
src/<NodeName>/
  ├── <NodeName>.h      # extends GraphNode
  ├── <NodeName>.cpp    # implements process()
  └── <NodeName>.json   # UI layout metadata (port positions, grid size)
```

1. Add a single entry to the registry table in `src/NodeFactory.cpp` (the `NodeTypeInfo` struct: name, factory lambda, category, tags, preview metadata). All of `createNode()`, `getAvailableNodeTypes()`, `getNodeCategories()`, `getNodeTags()`, and `getPreviewMetadata()` are driven from this table.
2. Override `getInputPortType(int)` / `getOutputPortType(int)` if the node handles CC or is type-agnostic.
3. If the node generates CC, override `getDeclaredCCNumber()`.
4. For macro-bindable parameters: declare `MacroParam` members in the header and call `addMacroParam(&myParam)` in the constructor. Use `resolveMacroFloat` / `resolveMacroInt` / `resolveMacroOffset` in `process()`.
5. Add tags to `NodeFactory::getNodeTags()` in `src/NodeFactory.h` — use the established vocabulary (`rhythm`, `melody`, `harmony`, `mpe`, `cc`, `random`, `polyphony`, `velocity`, `logic`, `tuning`).

### Node Categories

`NodeFactory::getNodeCategories()` returns nodes grouped for display in the module panel:

| Category | Nodes |
|----------|-------|
| I/O | Midi In, Midi Out |
| Modulation | CC Modulator |
| Generators | All Notes, ChordN, Sequence, Walk |
| Pattern | Converge, Diverge, Fold, Multiply, Reverse, Sort, Unfold |
| Combinatorial | And, Or, Xor, Chord Split, Concatenate, Interleave, Split, Unzip, Zip |
| Pitch & Range | Octave Stack, Octave Transpose, Quantizer, Transpose |
| Routing | Route, Select, Switch |
| Filter | MPE Filter, Velocity Filter |
| Utility | Diagnostic, Note, Note Large |

### UI Layer

| File | Role |
|------|------|
| `src/PluginEditor.h/cpp` | Top-level JUCE UI. Owns `GraphCanvas`, the 32 `MacroControl` widgets, `selectedMacro` state, and all macro wiring callbacks. |
| `src/GraphCanvas.h/cpp` | Renders the node graph with pan/zoom; handles cable drag gestures; propagates `selectedMacroPtr` and hover state to all `NodeBlock`s. |
| `src/NodeBlock.h/cpp` | JUCE component for a single node. Builds dynamic UI from `NodeLayout`, manages `sliderMacroInfos` / `buttonMacroInfos`, and draws all macro overlays in `paintOverChildren`. |
| `src/SharedMacroUI.h` | `CustomMacroSlider` and `CustomMacroButton` — controls with shift+drag binding gestures and hover callbacks. |
| `src/PluginProcessor.h/cpp` | JUCE `AudioProcessor` entry point; handles MIDI callback, CLAP protocol, undo/redo, `updateMacroNames()`. |
| `src/ModuleLibraryPanel.h/cpp` | Browsable panel showing all node types grouped by category. Search uses multi-token AND semantics matching against node name, category, and tags. |

### Supporting Systems

| File | Responsibility |
|------|---------------|
| `src/MacroColours.h` | 32-color palette for macro visual indicators |
| `src/MacroParameter.h` | Per-macro APVTS parameter (bipolar flag, display name) |
| `src/ClockManager` | DAW sync, PPQN tracking, standalone internal transport (play/pause/seek/loop) |
| `src/TimelineRuler` | Standalone mini-timeline UI: bar/beat grid, playhead, seek/jog, zoom, scroll/pan (Shift+wheel or middle-drag), loop region, and auto-follow keeping the playhead in the view's central band while playing |
| `src/NoteExpressionManager` | MPE channel assignment, round-robin voice allocation, expression passthrough |
| `src/PatchLibrary` | Save/load patch state to disk (XML, `.euclidya` extension) |
| `src/AppSettings` | User preferences |
| `src/Tuning/` | Scala/KBM microtonal tuning tables and UI panel |

## Key Documentation

- `DESIGN.md` — architectural deep-dive
- `CONTRIBUTING.md` — contribution workflow and coding standards
- `patches/Factory/` — bundled tutorial patches useful for manual testing
- User and developer docs: https://arps.chalkwalkmusic.com (Docusaurus site, synced to GitHub Wiki)
- Project repository: https://github.com/chalkwalk/arps-euclidya
