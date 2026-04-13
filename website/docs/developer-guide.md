# Developer Guide

Welcome to the Arps Euclidya codebase! The project uses C++17, JUCE, and CMake.

## Architectural Overview

The heart of the application is an instantaneous topological di-graph (Directed Acyclic Graph). Unlike DSP audio block routing (where every component processes audio every frame), our system uses a "Dirty Flag" calculation methodology.

Whenever a parameter is changed, a patch cable is dragged, or a MIDI key is struck, the Engine marks the source node as "Dirty". It then uses **Kahn's Topological Sort** to recalculate the sequence flow for every node down the chain instantaneously.

### Engine Subsystems (`src/`)

- **`GraphEngine.cpp/h`**: The primary data orchestrator. It holds references to all `GraphNode` subclasses. It performs the AABB grid occupancy checks (`isAreaOccupied`) for drag-and-drop mechanics, explicitly manages `GraphNode::Connection` maps, and implements `recalculate()` and `topologicalSort()`.
- **`GraphNode.cpp/h`**: The abstract base class from which all 27+ modules inherit. Each provides grid positioning variables (`gridX`, `gridY`) and a `process()` hook for the topological sort.
- **`MidiHandler.cpp/h`**: Maintains the global state of MPE channel mapping. It absorbs raw JUCE MidiMessages from the DAW, interprets MPE (Pitchbend, CC74, Pressure), converts them into continuous internal bounds, and feeds the `MidiInNode`.
- **`ClockManager.cpp/h`**: Syncs heavily modified states involving `getCumulativePpq()` (Pulses Per Quarter note) for seamless sub-tick synchronization and playhead monitoring.
- **`NodeBlock.cpp/h`**: The JUCE visual representation of the node. Inherits from JUCE `Component` but relies entirely on `GraphNode` for backend mathematics.

## Core Data Model

The single form of data passed across all cables is the **`NoteSequence`**, found in `src/DataModel.h`.

```cpp
struct MpeCondition {
  float xMin = 0.0f, xMax = 1.0f;  // Bend  (0.5 = centre, no bend)
  float yMin = 0.0f, yMax = 1.0f;  // Timbre / CC74
  float zMin = 0.0f, zMax = 1.0f;  // Pressure

  bool passes(float x, float y, float z) const;  // live test at playback time
  bool isPassThrough() const;                     // fast-exit when no filter upstream
  static bool tryHull(const MpeCondition &a, const MpeCondition &b,
                      MpeCondition &out);         // union if ranges touch or overlap
  void intersectX(float lo, float hi);            // narrows the X axis range
  void intersectY(float lo, float hi);
  void intersectZ(float lo, float hi);
};

struct HeldNote {
  int noteNumber = 0;
  int channel = 1;
  float velocity = 0.0f;
  float mpeX, mpeY, mpeZ;        // Pitch Bend, Timbre, Pressure (snapshot at note-on)
  MpeCondition mpeCondition;     // playback-time predicate; defaults to full-range passthrough
  // ...
};
using NoteSequence = std::vector<std::vector<HeldNote>>;
```

It represents a 2D matrix of "Time Steps", where each individual step contains a `vector` of `HeldNote`s (to natively represent chords occurring simultaneously on a single clock step).

### MpeCondition

`MpeCondition` is a **transient, per-note predicate** on the three MPE axes. It lives on `HeldNote` in the in-flight `NoteSequence` and is never serialized — loading a patch re-derives all conditions from `process()` calls.

The default condition (`isPassThrough() == true`) is a zero-cost fast path: `MidiOutNode` skips the live lookup entirely unless at least one filter node upstream has narrowed a range.

Filter nodes narrow conditions by calling `intersectX/Y/Z`. Impossible ranges (min > max after chaining two conflicting filters) are kept — they always fail `passes()` at playback, silencing the note. The `ZipNode` uses `tryHull` to merge complementary conditions back into a union range when the same pitch arrives from two branches with touching or overlapping ranges.

## Creating New Nodes

If you want to create a new module, you must:

1. Inherit from `GraphNode.h` and implement `process()`.
2. Determine input ports and output ports by overriding `getNumInputPorts()`.
3. Add it to `NodeFactory.h` (`createNode` instantiation, name list, and `getPreviewMetadata()`).
4. Recompile via the unified CMake structure.
