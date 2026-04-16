# Developer Guide

Welcome to the Arps Euclidya codebase! The project uses C++17, JUCE, and CMake.

## Architectural Overview

The heart of the application is an instantaneous topological di-graph (Directed Acyclic Graph). Unlike DSP audio block routing (where every component processes audio every frame), our system uses a "Dirty Flag" calculation methodology.

Whenever a parameter is changed, a patch cable is dragged, or a MIDI key is struck, the Engine marks the source node as "Dirty". It then uses **Kahn's Topological Sort** to recalculate the sequence flow for every node down the chain instantaneously.

### Engine Subsystems (`src/`)

- **`GraphEngine.cpp/h`**: The primary data orchestrator. It holds references to all `GraphNode` subclasses. It performs the AABB grid occupancy checks (`isAreaOccupied`) for drag-and-drop mechanics, explicitly manages `GraphNode::Connection` maps, and implements `recalculate()` and `topologicalSort()`.
- **`GraphNode.cpp/h`**: The abstract base class from which all 27+ modules inherit. Each provides grid positioning variables (`gridX`, `gridY`) and a `process()` hook for the topological sort.
- **`NoteExpressionManager.cpp/h`**: Maintains the global state of MPE channel mapping (formerly `MidiHandler`). It absorbs raw JUCE MidiMessages from the DAW, interprets MPE (Pitchbend, CC74, Pressure), converts them into continuous internal bounds, and feeds the `MidiInNode`.
- **`ClockManager.cpp/h`**: Syncs heavily modified states involving `getCumulativePpq()` (Pulses Per Quarter note) for seamless sub-tick synchronization and playhead monitoring.
- **`NodeBlock.cpp/h`**: The JUCE visual representation of the node. Inherits from JUCE `Component` but relies entirely on `GraphNode` for backend mathematics.

## Core Data Model

Inter-node data is carried as an **`EventSequence`**, found in `src/DataModel.h`. Each sequence is a vector of steps, where each step is a vector of `SequenceEvent` variants — either a `HeldNote` or a `CCEvent`:

```cpp
struct CCEvent {
    int   ccNumber = 0;    // 0..127
    float value    = 0.0f; // normalised 0..1
    int   channel  = 1;
};
using SequenceEvent  = std::variant<HeldNote, CCEvent>;
using EventStep      = std::vector<SequenceEvent>;
using EventSequence  = std::vector<EventStep>;
using NoteSequence   = EventSequence;  // backwards-compat alias
```

Two helpers make variant access concise throughout the codebase:

```cpp
inline const HeldNote* asNote(const SequenceEvent& e) { return std::get_if<HeldNote>(&e); }
inline const CCEvent*  asCC  (const SequenceEvent& e) { return std::get_if<CCEvent>(&e); }
```

A step holds either note events or CC events — never a mixture. This invariant is guaranteed by the generator nodes that produce sequences.

`HeldNote` is unchanged from before:

```cpp
struct HeldNote {
  int noteNumber = 0;
  int channel = 1;
  float velocity = 0.0f;
  float mpeX, mpeY, mpeZ;        // Pitch Bend, Timbre, Pressure (snapshot at note-on)
  MpeCondition mpeCondition;     // playback-time predicate; defaults to full-range passthrough
  // ...
};
```

### Port Types

Every node port declares a `PortType`:

```cpp
enum class PortType { Notes, CC, Agnostic };
```

- **Notes** (gold cable): pitch-semantic nodes like Transpose, Quantizer, Fold.
- **CC** (violet cable): `AlgorithmicModulatorNode` output; `MidiOutNode` Port 1.
- **Agnostic** (grey cable): structural nodes (Sort, Reverse, Split, Route, etc.) that work on either type. The cable adopts the type of the first connected edge.

Connecting a Notes port to a CC port is rejected at connection time with a red-flash indicator. See `GraphEngine::addExplicitConnection` and `GraphCanvas::endCableDrag`.

### MpeCondition

`MpeCondition` is a **transient, per-note predicate** on the three MPE axes. It lives on `HeldNote` in the in-flight `EventSequence` and is never serialized — loading a patch re-derives all conditions from `process()` calls.

The default condition (`isPassThrough() == true`) is a zero-cost fast path: `MidiOutNode` skips the live lookup entirely unless at least one filter node upstream has narrowed a range.

Filter nodes narrow conditions by calling `intersectX/Y/Z`. Impossible ranges (min > max after chaining two conflicting filters) are kept — they always fail `passes()` at playback, silencing the note. The `ZipNode` uses `tryHull` to merge complementary conditions back into a union range when the same pitch arrives from two branches with touching or overlapping ranges.

## Creating New Nodes

If you want to create a new module, you must:

1. Inherit from `GraphNode.h` and implement `process()`.
2. Determine port counts by overriding `getNumInputPorts()` / `getNumOutputPorts()`, and port types by overriding `getInputPortType()` / `getOutputPortType()` (defaults to `PortType::Notes`; override to `PortType::CC` or `PortType::Agnostic` as appropriate).
3. Add it to `NodeFactory.h` in three places: `createNode`, `getAvailableNodeTypes`, and `getPreviewMetadata`. Also place it in the correct category in `getNodeCategories` — this controls which section of the Module Library panel it appears under.
4. Recompile via the unified CMake structure.
