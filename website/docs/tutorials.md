# Tutorial Series

Arps Euclidya includes a series of factory patches designed to teach you the modular MIDI workflow from the ground up. You can find these in the `patches/Factory/` directory.

## Tutorial 0: Basic MIDI Routing

**Learning Goal**: Understand the basic flow from input to output.

- **Path**: `MIDI In` -> `Sort` -> `MIDI Out`
- **Concept**: MIDI notes enter the system, are sorted into ascending order (a clean starting point for arpeggiation), and then sent to the Euclidean Engine (`MIDI Out`).
- **Action**: Hold a chord and adjust the "Rhythm" and "Pattern" rings on the `MIDI Out` node.

## Tutorial 1: Sequence Transformations

**Learning Goal**: Manipulating the sequence order.

- **Path**: `MIDI In` -> `Sort` -> `Reverse` -> `MIDI Out`
- **Concept**: The `Reverse` node flips the sequence. In this tutorial, an ascending sorted chord becomes a descending one.
- **Action**: Try bypassing the `Reverse` node to hear the difference in arpeggio direction.

## Tutorial 2: Pattern Parallelism

**Learning Goal**: Layering multiple transformations.

- **Path**: One path goes `Sort -> Concatenate A`, another goes `Sort -> Reverse -> Concatenate B`.
- **Concept**: The `Concatenate` node joins two sequences end-to-end. Here, we create an "Up-Down" sweep by joining the forward and reversed versions of the same input.
- **Action**: Change the Euclidean rhythm to a longer pattern (e.g. 16 steps) to hear the full sweep.

## Tutorial 3: Chord Construction & Quantization

**Learning Goal**: Generating complex harmonies from single notes.

- **Path**: `Transpose (+4)`, `Transpose (-5)`, and original input are merged via `Zip` nodes, then sent to a `Quantizer`.
- **Concept**: `Zip` combines multiple single-note sequences into chords. The `Quantizer` (set to D Dorian) ensures that even if you play "wrong" notes, the resulting harmonies stay in key.
- **Action**: Play single notes and listen to the emergent 3-note chords.

## Tutorial 4: Generative Chords

**Learning Goal**: Using the `All Notes` node for autonomous patterns.

- **Path**: `All Notes` -> `Quantizer` -> `Symmetry Nodes (Converge/Diverge/Reverse)` -> `Zip` -> `MIDI Out`.
- **Concept**: This patch requires no MIDI input. It uses all 12 chromatic notes, quantizes them to a C Aeolian scale, and then uses symmetrical transformations to build complex, 4-note evolving chords.
- **Action**: Change the scale on the `Quantizer` to instantly morph the entire generative composition. If you load a microtonal tuning, the Quantizer's step grid will resize to match — unfold the node to author a custom scale for that tuning and save it for reuse.

## Tutorial 5: Multi-Channel Polyrhythms

**Learning Goal**: Routing different sequence parts to different instruments.

- **Path**: One sequence is split using `Chord Split` into top and bottom parts.
- **Concept**: Three independent `MIDI Out` nodes are used, each set to a different MIDI channel with unique Euclidean rhythms and time divisions.
- **Action**: Route MIDI Channels 1, 2, and 3 in your DAW to different instruments (e.g., Bass, Pads, Lead) to hear the polyrhythmic interplay.

## Tutorial 6: MPE & Expression

**Learning Goal**: Using per-note expression for dynamic performance.

- **Path**: `MPE MIDI In` -> `ChordN` -> `Symmetries` -> `MIDI Out` (with Pressure mapping).
- **Concept**: This tutorial demonstrates MPE integration. Per-note pressure is mapped to output velocity (`Pressure to Velocity` set to 0.5), allowing each note in the generated chords to have its own dynamic intensity.
- **Action**: Use an MPE controller (like a LinnStrument or Seaboard) and vary the pressure on individual fingers to "play" the arpeggio's dynamics.
