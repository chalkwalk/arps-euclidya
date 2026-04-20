# Arps Euclidya - Design Document

## 1. Overview

**Arps Euclidya** is a modular, di-graph based MIDI effect designed for expressive performance. It decouples melodic sequence generation from rhythmic triggering, utilizing Euclidean algorithms to create complex, evolving patterns while maintaining full MPE (MIDI Polyphonic Expression) support.

## 2. Core Architecture: The Modular Di-Graph

The plugin's logic is defined by an acyclic directed graph of modular nodes. Unlike linear arpeggiators, this architecture allows for complex routing, splitting, and merging of note sequences.

### 2.1 The Data Model: NoteSequence

The fundamental unit of data flowing between nodes is the `NoteSequence`.

- **HeldNote**: A struct containing the base MIDI note, channel, initial velocity, and per-note MPE dimensions (Pitch Bend, CC74/Timbre, and Pressure).
- **Sequence Structure**: Represented as `std::vector<std::vector<HeldNote>>`. This is an ordered list of "steps." Each step can contain zero notes (a rest) or multiple notes (a chord).

### 2.2 Instantaneous Recalculation

The graph operates on an "instantaneous computation" model:

- **Dirty Flags**: When a node's parameters or inputs change, it is marked as "dirty."
- **Topological Traversal**: The `GraphEngine` performs a topological sort to recalculate all downstream nodes in the correct order.
- **Caching**: Nodes cache their input and output sequences to avoid redundant computation.

### 2.3 Graph Mutations and Undo/Redo

All structural graph changes (adding/removing nodes or connections, modifying parameters that affect patch state) are routed through `PluginProcessor::performGraphMutation()`. This wraps the change in a `juce::UndoableAction` and commits it to `undoManager`, giving full undo/redo support across the entire session. UI components access mutations via a `GraphCanvas::performMutation` callback rather than touching the engine directly.

## 3. Engine Subsystems

### 3.1 Graph Engine (`src/GraphEngine.h/cpp`)

Orchestrates the lifecycle of nodes and their connections. It ensures the graph remains acyclic and manages the recalculation flow. After any graph structural change it also redistributes the macro atomic pointer array and `macroBipolarMask` to every node so macro resolution is always current.

### 3.2 Note Expression Manager (`src/NoteExpressionManager.h`)

`NoteExpressionManager` (formerly `MidiHandler`) maintains the global state of held notes and active MPE channels using a `juce::MPEInstrument` internally. It serves as the primary data source for `MidiInNode` — translating incoming MIDI messages into the `HeldNote` structures that flow through the graph — and provides real-time per-note expression polling for `MidiOutNode` at output time.

### 3.3 Clock Manager (`src/ClockManager.h`)

Handles synchronization with the DAW host. It tracks PPQN (Pulses Per Quarter Note) and manages transport-related events, ensuring nodes can react to beat divisions and playhead resets.

### 3.4 Macro System

The macro system exposes 32 global automatable parameters to the DAW host while allowing each one to modulate any number of internal node parameters additively, with a signed intensity and per-macro polarity.

#### Two-Type Architecture

- **`MacroParameter`** (`src/MacroParameter.h`): A `juce::AudioProcessorParameter` subclass, one per slot, registered in the APVTS and visible to the DAW. Carries the current value `[0, 1]`, a display name (set dynamically by `updateMacroNames()` to reflect what the macro controls), and a `bipolar` flag.

- **`MacroParam`** (`src/GraphNode.h`): A lightweight struct `{ juce::String name; std::vector<MacroBinding> bindings; }` that lives on each node. `MacroBinding` is `{ int macroIndex; float intensity; }`. Nodes own their `MacroParam` members and register them by calling `addMacroParam()` in their constructor. `GraphNode::getMacroParams()` returns the full registered list.

#### Macro Distribution

`GraphEngine` holds pointers to the 32 `std::atomic<float>` values inside the APVTS parameters and an `std::atomic<uint32_t> macroBipolarMask`. Both are written to every `GraphNode` after any structural graph change. Nodes read them atomically in `process()` — no locking required.

#### Additive Resolution

Nodes compute effective parameter values using `resolveMacroFloat` or `resolveMacroInt`, both defined on `GraphNode`:

```text
for each binding in MacroParam:
    contribution = isBipolar
        ? (macroVal - 0.5) × 2 × intensity × range   // bipolar: centre = no change
        :  macroVal × intensity × range               // unipolar: 0 = no change
effective = clamp(localVal + Σ contributions, min, max)
```

Intensity is a signed float `[-2, 2]`. Values outside `±1` allow "super-scaling" beyond the parameter's natural range (clamped at the parameter boundary).

#### Binding Notifications

`GraphNode::onMappingChanged` is a `std::function<void()>` callback set by `PluginProcessor`. Whenever bindings change, UI components call it; this triggers `updateMacroNames()` which scans all nodes via `getMacroParams()`, rebuilds each `MacroParameter`'s display name to reflect its current targets, and sets an `std::atomic<bool> macrosDirty` flag polled by the editor timer.

#### CC Mapping and MIDI Learn

The interface provides MIDI learn functionality to map external MIDI CC messages directly to panel controls and macros. This system integrates intimately with the macro architecture, allowing any bindable knob or macro slot to quickly learn incoming CC values, overriding local or automated values when physical hardware is adjusted.

### 3.5 UI Layout System (`src/NodeLayout.h`, `src/LayoutParser.h`)

Node UI is data-driven. Each node ships a companion `<NodeName>.json` file that declares the grid footprint and a list of `UIElement` entries. The `LayoutParser` reads these at runtime and `NodeBlock` instantiates the corresponding JUCE widgets, wiring `valueRef` / `macroParamRef` pointers directly to the node's member variables.

`UIElementType` covers: `RotarySlider`, `Toggle`, `Label`, `ComboBox`, `PushButton`, and `Custom` (for nodes with fully bespoke editors). Nodes may also declare an *unfolded* layout (a collapsible panel) for secondary controls, which supports expansion in all directions (up, down, left, right) and includes tabbing support for richer organization of advanced parameters.

This separation keeps node logic (`process()`) independent of UI and makes adding controls to a node purely a data change.

### 3.6 Patch Library (`src/PatchLibrary.h`)

Patches are serialized as XML to disk. `GraphEngine` drives `saveNodeState` / `loadNodeState` on each node (including macro bindings via `saveMacroBindings` / `loadMacroBindings`), and the top-level XML records the connection graph, macro polarities, and canvas position for every node. Patches are stored in a user-configurable directory and displayed in a browsable panel with name, description, and tags.

## 4. Node Dictionary

The system provides 32 node types, categorized by their role in the processing pipeline. Every node carries an `EventSequence` input/output (except pure generation nodes). Nodes marked *(Macros)* expose one or more parameters to the macro system via `MacroParam` members.

### 4.1 Core I/O & Generation

- **Midi In Node** *(Macros)*: The entry point for live performance. Captures notes and MPE data from the DAW or the on-screen keyboard. Supports channel filtering.
- **Midi Out Node** *(Macros)*: The terminal destination. Implements the **Euclidean Engine** (Pattern & Rhythm separation), sends real-time MIDI note and CC events to the host. Features a **Gate %** control (1–150% of clock division) and **Flex Gate** mode (holds pitches across adjacent steps instead of retriggering), plus Humanize parameters (timing, velocity, gate jitter), realtime MPE controls (ratchet and octave jump), and an interactive circular visualizer. Accepts a CC sequence on Port 1 (violet) and provides per-CC# anchor, slew, and rest-behaviour controls in the registry panel. On transport stop, held notes decay within one clock division rather than cutting off hard.
- **Sequence Node** *(Macros)*: A 16×128 "piano roll" generator. Users draw patterns on a scrollable grid; the pattern plays independently of held keys. Sequence length is macro-bindable.
- **All Notes Node**: Outputs every MIDI note (0–127) as a single sequence step per note. Useful as a source for `Fold` or scale-quantizer chains.
- **CC Modulator Node** *(Macros)*: A CC sequence generator. Produces a CC `EventSequence` from an algorithm (Sine, RampUp, RampDown, Triangle, RandomHold, RandomWalk, EuclideanGates, Custom). CC values are normalised 0..1 internally. The Algorithm selector is macro-bindable.
- **Diagnostic Node**: Displays step count, chord density, and basic stats of the incoming sequence. Used for debugging patch flow. Accepts both Notes and CC sequences (Agnostic port).
- **Text Note Node**: A canvas annotation — stores a free-text label with no signal processing. Useful for documenting patches.

### 4.2 Pattern & Order (Directional)

Nodes in this category declare **Agnostic** ports and operate identically on Notes and CC sequences.

- **Sort Node**: Reorders steps by velocity-weighted mean pitch (notes) or mean CC value (CC).
- **Reverse Node**: Flips the sequence order.
- **Walk Node** *(Macros)*: Applies a weighted random walk to select steps from the sorted sequence. `Walk Length` sets the number of steps generated; `Walk Skew` biases the walk direction.
- **Converge Node**: Reorders steps from outside-in (alternating highest/lowest remaining).
- **Diverge Node**: Reorders steps from inside-out (opposite of Converge).
- **Select Node** *(Macros)*: Passes Input 0 or Input 1 through to the output.
- **Multiply Node** *(Macros)*: Repeats each step N times consecutively.
- **Fold Node** *(Macros)*: Folds the sequence timeline into chords. Can aggregate sequences in chunks (N steps into 1) or via a rolling window.
- **Unfold Node** *(Macros)*: Unfolds chords sequentially into an arpeggiated timeline, with options for ascending/descending order and note-dropping biases.

### 4.3 Combinatorial & Chord Logic

Nodes in this category also declare **Agnostic** ports unless noted.

- **Chord N Node** *(Macros)*: Extracts exhaustive N-note combinations from the input chord. Creates complex polyphonic variations from simple held keys. *(Notes port — pitch-semantic)*
- **Chord Split Node**: Extracts the top note to Output 0 and the bottom note to Output 1. *(Agnostic)*
- **Octave Stack Node** *(Macros)*: Expands the sequence by adding octave-shifted copies, with deduplication. *(Notes port — pitch-semantic)*
- **Concatenate Node**: Appends Sequence B to the end of Sequence A. *(Agnostic)*
- **Split Node** *(Macros)*: Divides a sequence into two parts based on percentage, fixed count, or note range. *(Agnostic)*
- **Zip Node**: Merges two parallel sequences into one by stacking matching steps. *(Agnostic)*
- **Unzip Node**: Splits a sequence into two lines — lowest-value steps to Output 0, highest to Output 1. *(Agnostic)*
- **Interleave Node**: Interleaves steps from two input sequences alternately. *(Agnostic)*
- **And Node**: Outputs only the notes whose pitch appears in **both** inputs. Highest-velocity version of each matched pitch is emitted; `MpeCondition` is narrowed to the intersection of both predicates. Length policy (Pad/Truncate) is switchable. *(Notes port)*
- **Or Node**: Outputs the **union** of notes from both inputs. Pitches present in both carry the highest velocity with a hulled `MpeCondition`; pitches in only one input pass through unchanged. Length policy (Pad/Truncate) is switchable. *(Notes port)*
- **Xor Node**: Outputs notes whose pitch appears in **exactly one** input. Surviving notes pass through with their original `MpeCondition`. Length policy (Pad/Truncate) is switchable. *(Notes port)*

### 4.4 Pitch & Range Manipulation

These nodes declare **Notes** ports — they interpret pitch numerically and should not receive CC sequences.

- **Transpose Node** *(Macros)*: Shifts the entire sequence by a fixed semitone interval.
- **Octave Transpose Node** *(Macros)*: Shifts by full octaves.
- **Quantizer Node** *(Macros)*: Constrains all pitches to a specific musical scale and root. Scale mode and rest-on-drop behavior are macro-bindable.

### 4.5 Routing & Logic

Agnostic ports — work identically on Notes and CC sequences.

- **Route Node** *(Macros)*: Sends the input sequence to Output 0 or Output 1 exclusively. Useful for switching between song sections.
- **Switch Node** *(Macros)*: An on/off mute gate for the entire sequence path.

## 5. The Euclidean Engine

Located within the `MidiOutNode`, the Euclidean Engine is the rhythmic core of the plugin. It uses two decoupled layers of Euclidean arrays to determine *when* and *what* plays:

1. **The Pattern (Note vs. Skip)**: Uses *Steps*, *Beats*, and *Offset* to decide if the next note in the input `NoteSequence` should be played or skipped. Skipping advances the sequence pointer without triggering output.
2. **The Rhythm (Beat vs. Rest)**: Uses its own *Steps*, *Beats*, and *Offset* to define triggers on the clock grid. "Beats" trigger the pattern engine; "Rests" produce silence.

This dual-layer approach allows for complex syncopation and polymetric behavior that remains harmonically grounded in the input sequence.

## 6. Look & Feel: "Neon Slate"

The interface uses a high-contrast "Neon Slate" visual language: deep charcoal backgrounds with cyan, magenta, and color-coded accents for active states, signal flow, and macro indicators. The canvas is hardware-accelerated via JUCE's OpenGL context.

### 6.1 The Modular Canvas

- **Grid-Centric**: Nodes snap to a fixed grid. A subtle background grid scales with zoom level.
- **Infinite Space**: The workspace supports seamless pan (click-drag on empty space) and smooth zoom (`Ctrl + Scroll`). Scroll bars track viewport position.
- **Cable Rendering**: Patch cables are drawn as Bézier curves in `GraphCanvas::paintOverChildren`, rendered above all node bodies. Cable color encodes state: normal, active, warning (orange/red for sequences exceeding 10,000 steps).
- **Node Selection**: Clicking a node brings it to the front and highlights its cables.

### 6.2 Advanced Patching Gestures

- **Cable Insertion (Splice)**: Dragging a new node over an active cable highlights the wire and inserts the node into that connection on drop.
- **Edge-Drop Auto-Connect**: Dropping a node onto the left edge of an existing node connects output→input; dropping on the right edge does the reverse.
- **Library Search**: A collapsible sidebar with multi-token search. Tokens are matched case-insensitively against node name, category, and tags (AND semantics across tokens). Pressing `Enter` spawns the top result at the mouse cursor.
- **Node Replace**: Right-clicking a node header exposes "Replace with…" — substitutes the node type while preserving compatible connections.
- **Node Bypass**: Every node has a bypass toggle in its header that passes the input sequence unchanged without altering the patch graph.
- **Undo/Redo**: All structural changes (node add/remove, connections, parameter mutations) are undoable with `Ctrl+Z` / `Ctrl+Y`.

### 6.3 Macro Visuals

The macro system has a rich set of visual indicators that keep the binding state legible at a glance:

- **32 color-coded palette slots** run along the top of the plugin window. Each macro has a unique hue from a fixed 32-color HSL wheel (`src/MacroColours.h`).
- **Selecting a macro** (click) glows its slot in full color and draws a faint matching ring on every unbound parameter knob on the canvas, signaling that shift+drag will bind it.
- **Intensity arcs** appear outside bound knobs — arc length encodes binding intensity, concentric rings for multiple bindings, each in its macro's color.
- **Effective-value indicator**: A white bridge arc and hollow ring on the knob face show the parameter's current effective value (after all macro contributions), moving in real time.
- **Button indicators**: Bound toggle buttons show a colored border ring (per binding), an intensity bar along the bottom, and a dot when the macro is currently overriding the local state.
- **Hover — control → palette**: Hovering a bound knob or button highlights the macro slots it is bound to with a white ring.
- **Hover — palette → canvas**: Hovering a macro slot highlights all controls bound to it across the canvas with white rings.
- **Bipolar toggle**: Double-clicking a macro slot switches it between unipolar (0 → full contribution) and bipolar (centre = no contribution, ±full at extremes). A `±` mark and a 12-o'clock tick appear on the knob.

### 6.4 Interactive Feedback

- **Euclidean Rings**: The `MidiOutNode` features a large, interactive ring visualization. Users can drag rings to adjust Euclidean offsets or toggle individual steps directly in the UI.
- **Cable Tooltips**: Hovering over any cable displays a tooltip showing the current `NoteSequence` step count and active note count, with orange/red warnings for dangerously large sequences.
- **Ghost Placement**: While dragging a node, a ghost outline previews the resolved snap target and changes color to indicate whether the target grid cell is occupied.

## 7. Event Sequence Model & Port Types

### 7.1 Variant Carrier (`src/DataModel.h`)

The fundamental unit of data flowing between nodes is no longer a homogeneous vector of `HeldNote`s. Each inter-node edge carries an `EventSequence` — a vector of `EventStep`s where every step is a vector of `SequenceEvent` variants:

```cpp
struct CCEvent {
    int   ccNumber = 0;    // 0..127
    float value    = 0.0f; // normalised 0..1 (converted to 0..127 only at MIDI emission)
    int   channel  = 1;
};
using SequenceEvent  = std::variant<HeldNote, CCEvent>;
using EventStep      = std::vector<SequenceEvent>;
using EventSequence  = std::vector<EventStep>;
using NoteSequence   = EventSequence;  // backwards-compat alias
```

Two helper functions make variant access concise throughout the codebase:

```cpp
inline const HeldNote* asNote(const SequenceEvent& e) { return std::get_if<HeldNote>(&e); }
inline const CCEvent*  asCC  (const SequenceEvent& e) { return std::get_if<CCEvent>(&e); }
```

**One step, one type.** A step holds either note events or CC events, never a mixture. This invariant is guaranteed by the generators that produce sequences, not enforced at the container level.

### 7.2 Port Types (`src/GraphNode.h`)

Every node port declares a `PortType` that governs cable compatibility:

```cpp
enum class PortType { Notes, CC, Agnostic };
virtual PortType getInputPortType(int port) const;   // default: Notes
virtual PortType getOutputPortType(int port) const;  // default: Notes
```

- **Notes** (gold cable): carries only `HeldNote` steps. Nodes with pitch-semantic logic (Transpose, Quantizer, Fold, Walk, etc.) declare gold ports to prevent accidental CC routing.
- **CC** (violet cable): carries only `CCEvent` steps. `AlgorithmicModulatorNode`'s output and `MidiOutNode`'s CC input port are violet.
- **Agnostic** (grey/white cable): accepts either type. All pure reorder/route nodes (Sort, Reverse, Converge, Diverge, Split, Concatenate, ChordSplit, Multiply, Zip, Unzip, Interleave, Select, Route, Switch, Diagnostic) are Agnostic — they operate structurally without interpreting the event payload.

### 7.3 Connection Compatibility

When a cable is dropped, `GraphEngine::addExplicitConnection` and `GraphCanvas::endCableDrag` both enforce:

- Notes → CC or CC → Notes: **rejected** (red flash at the drop point).
- Agnostic ports latch to the type of the first connected edge. A second edge on that port must match the latched type.

Cable colour is determined by the **source port's** effective type (latched type for Agnostic). The `GraphCanvas::drawCable` function queries the source node's `getOutputPortType` and colours accordingly.

### 7.4 `AlgorithmicModulatorNode` (CC Generator)

A generator node with no inputs and one CC output. It builds an `EventSequence` of configurable length from one of several algorithms (Sine, RampUp, RampDown, Triangle, RandomHold, RandomWalk, EuclideanGates, Custom). CC values are stored as normalised floats 0..1 and converted to the 7-bit range 0..127 only at `MidiOutNode` output time.

### 7.5 `MidiOutNode` CC Input

`MidiOutNode` has a second input port (Port 1, violet) that accepts a CC sequence. It advances a `ccIndex` on the same tick as the note `sequenceIndex`, producing a shared step clock. Per-CC# state is tracked in a `CCLaneState` registry (anchor, slew, rest behaviour). Slew interpolation smooths CC transitions within a step — at slew = 0 the value jumps immediately; at slew = 1 the value ramps linearly across one full step duration. CC output is de-duplicated by 7-bit integer value before emission to avoid redundant MIDI messages.
