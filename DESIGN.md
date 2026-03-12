# Arps Euclidya - Design Document

## 1. Overview

The **Arps Euclidya** is a modular, di-graph based MIDI effect designed for expressive performance. It decouples melodic sequence generation from rhythmic triggering, utilizing Euclidean algorithms to create complex, evolving patterns while maintaining full MPE (MIDI Polyphonic Expression) support.

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

## 3. Engine Subsystems

### 3.1 Graph Engine

Orchestrates the lifecycle of nodes and their connections. It ensures the graph remains acyclic and manages the recalculation flow.

### 3.2 MIDI Handler

The `MidiHandler` maintains the global state of held notes and active MPE channels. It serves as the primary data source for the `MidiInNode` and provides real-time MPE polling for the `MidiOutNode`.

### 3.3 Clock Manager

Handles synchronization with the DAW host. It tracks PPQN (Pulses Per Quarter Note) and manages transport-related events, ensuring nodes can react to beat divisions and playhead resets.

### 3.4 Macro System

Exposes 32 global VST parameters (Macros) that can be mapped to internal node settings. This allows for complex performance automation within a DAW while maintaining the flexibility of the modular graph.

## 4. Node Dictionary

The system provides a diverse library of nodes, categorized by their role in the processing pipeline. Every node supports a `NoteSequence` input/output (except generation nodes).

### 4.1 Core I/O & Generation

- **Midi In Node**: The entry point for live performance. Captures notes and MPE data from the DAW or the on-screen keyboard.
- **Midi Out Node**: The terminal destination. Implements the **Euclidean Engine** (Pattern & Rhythm separation) and sends real-time MIDI events to the host. Features an interactive circular visualizer.
- **Sequence Node**: A 16-step "piano roll" generator. Allows users to draw patterns that play independently of held keys, useful for static rhythmic foundations.
- **All Notes Node**: Utility node that outputs every possible MIDI note (0-127). Often used as a source for `Fold` or `Transpose` chains.

### 4.2 Pattern & Order (Directional)

- **Sort Node**: Reorders the incoming sequence into ascending pitch (Up).
- **Reverse Node**: Flips the sequence order (Down).
- **Walk Node**: Implements a "Brownian motion" logic. Steps through the sequence with configurable probabilities for stepping forward, staying, or stepping backward.
- **Converge Node**: Reorders the sequence from outside-in (e.g., [1, 12, 2, 11...]).
- **Diverge Node**: Reorders the sequence from inside-out (opposite of Converge).

### 4.3 Combinatorial & Chord Logic

- **Chord N Node**: Extracts exhaustive N-note combinations from the input chord. Creates complex polyphonic variations from simple held keys.
- **Octave Stack Node**: Expands the sequence by adding octave offsets with intelligent uniqueness rules to avoid duplicate pitches.
- **Multiply Node**: Repeats each step in the sequence N times sequentially.
- **Concatenate Node**: Appends two sequences together end-to-end.
- **Zip Node**: Merges two parallel sequences into one by stacking matching steps into chords.
- **Unzip Node**: Splits a sequence into two, routing the lower notes of chords to Output 0 and higher notes to Output 1.

### 4.4 Pitch & Range Manipulation

- **Transpose Node**: Shifts the entire sequence by a fixed semitone interval.
- **Octave Transpose Node**: Shifts by full octaves.
- **Quantizer Node**: Constrains all pitches in the sequence to a specific musical scale.
- **Fold Node**: "Wraps" notes outside a certain range back into it (octave-normalized).
- **Unfold Node**: Expands the pitch range using a specific expansion algorithm.

### 4.5 Routing & Logic

- **Split Node**: Divides a sequence into two parts based on percentage, fixed count, or note range.
- **Chord Split Node**: Extracts the "Top" or "Bottom" note from every chord into a separate output.
- **Route Node**: Sends the input sequence to one of two potential outputs based on a control value.
- **Select Node**: Picks between two input sequences based on a control value.
- **Switch Node**: An on/off gate for the entire sequence.

## 5. The Euclidean Engine

Located within the `MidiOutNode`, the Euclidean Engine is the rhythmic brain of the plugin. It uses two decoupled layers of Euclidean arrays to determine *when* and *what* plays:

1. **The Pattern (Note vs. Skip)**: Uses *Steps*, *Beats*, and *Offset* to decide if the next note in the input `NoteSequence` should be played or skipped. Skipping advances the sequence pointer invisibly.
2. **The Rhythm (Beat vs. Rest)**: Uses its own *Steps*, *Beats*, and *Offset* to define triggers on the clock grid. "Beats" trigger the pattern; "Rests" produce silence.

This dual-layer approach allows for complex syncopation and polymetric behavior that remains harmonically grounded in the input sequence.

## 6. Functional Look & Feel

The final vision for Arps Euclidya is a premium, high-tech modular environment inspired by Bitwig's "The Grid."

### 6.1 Visual Language: "Neon Slate"

- **Palette**: A base of deep charcoal (#121212) and slate grey, accented by high-contrast neon highlights (Cyan, Magenta, Lime) to indicate active states and signal flow.
- **Typography**: Clean, high-legibility sans-serif fonts (e.g., Roboto or Inter).
- **Nodes**: Semi-opaque modules with subtle drop shadows and rounded corners. Parameters use custom-drawn "flat arc" rotary sliders.

### 6.2 The Modular Canvas

- **Grid-Centric**: Nodes snap to a strictly defined grid. Background rendering features a subtle hairline grid that scales with zoom levels.
- **Infinite Space**: The workspace is a seamless `viewport` allowing infinite panning and smooth, hardware-accelerated zooming (Cmd/Ctrl + Scroll).
- **Z-Index Hierarchy**: Patch cables are rendered globally *above* all node bodies, using multi-pass Bézier drawing for a "glowing" bloom effect.

### 6.3 Advanced Patching Gestures

The interface prioritizes flow and performance over menu-diving:

- **Cable Insertion**: Dragging a node over an active patch cable highlights the wire and "splices" the node into that connection automatically.
- **Edge-Drop Auto-Connect**: Dropping a node onto the left edge of another node connects its outputs to the target's inputs. Dropping on the right edge does the reverse.
- **Library Search**: A collapsible sidebar with a "fuzzy-search" library. Pressing `Enter` on a single result instantly spawns the node at the mouse cursor.

### 6.4 Interactive Feedback

- **Euclidean Rings**: The `MidiOutNode` features a large, interactive ring visualization. Users can drag the ring to adjust Euclidean offsets or toggle steps directly in the UI.
- **Signal Visualization**: Hovering over any cable displays a tooltip showing the current `NoteSequence` step count (e.g., "12 Steps"), with orange/red warnings for dangerously long sequences (>10,000 steps).
