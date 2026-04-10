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

```
for each binding in MacroParam:
    contribution = isBipolar
        ? (macroVal - 0.5) × 2 × intensity × range   // bipolar: centre = no change
        :  macroVal × intensity × range               // unipolar: 0 = no change
effective = clamp(localVal + Σ contributions, min, max)
```

Intensity is a signed float `[-2, 2]`. Values outside `±1` allow "super-scaling" beyond the parameter's natural range (clamped at the parameter boundary).

#### Binding Notifications

`GraphNode::onMappingChanged` is a `std::function<void()>` callback set by `PluginProcessor`. Whenever bindings change, UI components call it; this triggers `updateMacroNames()` which scans all nodes via `getMacroParams()`, rebuilds each `MacroParameter`'s display name to reflect its current targets, and sets an `std::atomic<bool> macrosDirty` flag polled by the editor timer.

### 3.5 UI Layout System (`src/NodeLayout.h`, `src/LayoutParser.h`)

Node UI is data-driven. Each node ships a companion `<NodeName>.json` file that declares the grid footprint and a list of `UIElement` entries. The `LayoutParser` reads these at runtime and `NodeBlock` instantiates the corresponding JUCE widgets, wiring `valueRef` / `macroParamRef` pointers directly to the node's member variables.

`UIElementType` covers: `RotarySlider`, `Toggle`, `Label`, `ComboBox`, `PushButton`, and `Custom` (for nodes with fully bespoke editors). Nodes may also declare an *extended* layout (a collapsible panel) for secondary controls.

This separation keeps node logic (`process()`) independent of UI and makes adding controls to a node purely a data change.

### 3.6 Patch Library (`src/PatchLibrary.h`)

Patches are serialized as XML to disk. `GraphEngine` drives `saveNodeState` / `loadNodeState` on each node (including macro bindings via `saveMacroBindings` / `loadMacroBindings`), and the top-level XML records the connection graph, macro polarities, and canvas position for every node. Patches are stored in a user-configurable directory and displayed in a browsable panel with name, description, and tags.

## 4. Node Dictionary

The system provides 27 node types, categorized by their role in the processing pipeline. Every node carries a `NoteSequence` input/output (except pure generation nodes). Nodes marked *(Macros)* expose one or more parameters to the macro system via `MacroParam` members.

### 4.1 Core I/O & Generation

- **Midi In Node** *(Macros)*: The entry point for live performance. Captures notes and MPE data from the DAW or the on-screen keyboard. Supports channel filtering.
- **Midi Out Node** *(Macros)*: The terminal destination. Implements the **Euclidean Engine** (Pattern & Rhythm separation) and sends real-time MIDI events to the host. Features Humanize parameters (timing, velocity, gate) and an interactive circular visualizer.
- **Sequence Node** *(Macros)*: A 16×128 "piano roll" generator. Users draw patterns on a scrollable grid; the pattern plays independently of held keys. Sequence length is macro-bindable.
- **All Notes Node**: Outputs every MIDI note (0–127) as a single sequence step per note. Useful as a source for `Fold` or scale-quantizer chains.
- **Diagnostic Node**: Displays step count, chord density, and basic stats of the incoming sequence. Used for debugging patch flow.
- **Text Note Node**: A canvas annotation — stores a free-text label with no signal processing. Useful for documenting patches.

### 4.2 Pattern & Order (Directional)

- **Sort Node**: Reorders the incoming sequence into ascending pitch.
- **Reverse Node**: Flips the sequence order (descending pitch).
- **Walk Node** *(Macros)*: Applies a weighted random walk to select steps from the sorted sequence. `Walk Length` sets the number of steps generated; `Walk Skew` biases the walk direction (negative = prefer lower pitches, positive = prefer higher).
- **Converge Node**: Reorders the sequence from outside-in (alternating highest and lowest remaining notes).
- **Diverge Node**: Reorders the sequence from inside-out (opposite of Converge).

### 4.3 Combinatorial & Chord Logic

- **Chord N Node** *(Macros)*: Extracts exhaustive N-note combinations from the input chord. Creates complex polyphonic variations from simple held keys.
- **Chord Split Node**: Extracts the top note to Output 0 and the bottom note to Output 1. Useful for splitting a chord into independent bassline and melody paths.
- **Octave Stack Node** *(Macros)*: Expands the sequence by adding octave-shifted copies, with deduplication to avoid clustered pitches.
- **Multiply Node** *(Macros)*: Repeats each step in the sequence N times consecutively.
- **Concatenate Node**: Appends Sequence B to the end of Sequence A.
- **Zip Node**: Merges two parallel sequences into one by stacking matching steps into chords.
- **Unzip Node**: Splits a chord sequence into two mono lines — lowest notes to Output 0, highest to Output 1.

### 4.4 Pitch & Range Manipulation

- **Transpose Node** *(Macros)*: Shifts the entire sequence by a fixed semitone interval.
- **Octave Transpose Node** *(Macros)*: Shifts by full octaves.
- **Quantizer Node** *(Macros)*: Constrains all pitches to a specific musical scale and root. Scale mode and rest-on-drop behavior are macro-bindable.
- **Fold Node** *(Macros)*: "Wraps" notes that drift outside a target octave range back into it.
- **Unfold Node**: Expands the pitch range iteratively by pushing subsequent steps into higher or lower sub-octaves.

### 4.5 Routing & Logic

- **Split Node**: Divides a sequence into two parts based on percentage, fixed count, or note range.
- **Route Node** *(Macros)*: Sends the input sequence to Output 0 or Output 1 exclusively, based on an automatable control value. Useful for switching between song sections.
- **Select Node** *(Macros)*: Passes Input 0 or Input 1 through to the output, based on a control value.
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
- **Library Search**: A collapsible sidebar with fuzzy search. Pressing `Enter` spawns the top result at the mouse cursor.
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
