# Node Dictionary

The Arps Euclidya environment offers 32 specialized nodes, broken down into functional categories. Nodes marked `(Macros)` expose one or more parameters to the macro system — select a macro in the Macro Panel, then alt+drag the parameter knob (or alt+click a toggle button) to create a binding. Additionally, all nodes feature a **Bypass toggle** in their UI header to easily A/B test processing behavior without altering patch cables.

Patch cables come in three colours that indicate what type of data is flowing:

- **Gold** — note sequences (`HeldNote` events)
- **Violet** — CC sequences (`CCEvent` values, normalised 0–1)
- **Grey/White** — agnostic connections (the cable takes the colour of the first sequence type routed into it)

## Core I/O & Generation

- **Midi In Node**: The entry point for live performance. Captures notes and MPE data from the DAW or the on-screen keyboard. *(No Inputs, Outputs: 1)*
- **Midi Out Node `(Macros)`**: The terminal destination and home of the Euclidean Engine. Converts sequenced note and CC data into real-time MIDI events for the host. Port 0 (gold) accepts note sequences; Port 1 (violet) accepts a CC sequence and drives a per-CC# registry with anchor, slew, and rest-behaviour controls. Features four **Sync Modes** (Gestural, Synchronized, Deterministic, Forgiving) — see [The Euclidean Engine](euclidean-engine.md#global-timings--sync-modes) for details. Provides a **Gate %** knob (1–150% of the clock division) to set note duration, **Flex Gate** to hold pitches across adjacent steps instead of retriggering, "Humanize" parameters (timing jitter, velocity, gate jitter), and realtime MPE controls (ratchet and octave jump). On transport stop, held notes decay within one clock division. *(Inputs: 2, No Outputs)*
- **Sequence Node `(Macros)`**: A 16-step "piano roll" generator. Allows users to draw static patterns that play continuously, independent of held keys. *(No Inputs, Outputs: 1)*
- **All Notes Node**: A utility node that steadily outputs every possible MIDI note (C-2 to G8). Useful as an input signal for test routing or scale folding. *(No Inputs, Outputs: 1)*
- **Diagnostic Node**: Displays step count, note pitches, velocities, active MPE conditions, and CC values for each step. Accepts both note and CC sequences (agnostic port). *(Inputs: 1, Outputs: 1)*

## Modulation

- **CC Modulator Node `(Macros)`**: A CC sequence generator. Produces a stream of MIDI CC events from an algorithm — Sine, Ramp Up, Ramp Down, Triangle, Random Hold, Random Walk, Euclidean Gates, or Custom step-draw. CC values are stored normalised (0–1) and converted to 0–127 only at the Midi Out Node. Wire its output (violet) to the CC input of a Midi Out Node. The Algorithm selector is macro-bindable. *(No Inputs, Outputs: 1)*

## Pattern & Order (Directional)

- **Sort Node**: Reorders the incoming sequence by grouping and sorting notes into ascending pitch. Perfect for traditional "Up" arpeggios. *(Inputs: 1, Outputs: 1)*
- **Reverse Node**: Flips the chronological order of the steps. Creates a "Down" arpeggio. *(Inputs: 1, Outputs: 1)*
- **Walk Node `(Macros)`**: Applies a "Brownian motion" logic to the sequence. You can configure the probability of stepping forward, staying on the same step, or stepping backward. *(Inputs: 1, Outputs: 1)*
- **Converge Node**: Reorders steps from outside-in — alternating the highest and lowest remaining values. Works on both note and CC sequences (agnostic). *(Inputs: 1, Outputs: 1)*
- **Diverge Node**: Reorders steps from inside-out — the inverse of Converge. Works on both note and CC sequences (agnostic). *(Inputs: 1, Outputs: 1)*

## Combinatorial & Chord Logic

- **Chord N Node `(Macros)`**: Generates exhaustive N-note combinations from the incoming sequence or held chords. Transforms simple 4-note input chords into evolving chord options. *(Inputs: 1, Outputs: 1)*
- **Chord Split Node**: Extracts the "Top" note to Output A, and the "Bottom" note to Output B. Excellent for creating separate basslines and melodies from the same chord pad. *(Inputs: 1, Outputs: 2)*
- **Octave Stack Node `(Macros)`**: Duplicates the sequence pattern and stacks it at user-defined octave offsets, creating thick textures. Ensures no duplicate pitches are clustered. *(Inputs: 1, Outputs: 1)*
- **Multiply Node `(Macros)`**: Stretches the sequence by repeating each step N times consecutively. *(Inputs: 1, Outputs: 1)*
- **Concatenate Node**: Appends Sequence B to the end of Sequence A. Often used with a Sort and Reverse node to build a classic "Up/Down" arpeggiator. *(Inputs: 2, Outputs: 1)*
- **Interleave Node**: Alternates steps from two inputs: A[0], B[0], A[1], B[1], … If the inputs differ in length, the shorter is padded with rests. Output length = 2 × max(len(A), len(B)). *(Inputs: 2, Outputs: 1)*
- **And Node**: Outputs only notes whose pitch appears in **both** inputs. The highest-velocity version of each matched pitch is emitted, with the MPE condition narrowed to the intersection of both inputs. A **Pad/Truncate** mode controls how unequal lengths are handled. *(Inputs: 2, Outputs: 1)*
- **Or Node**: Outputs the **union** of pitches from both inputs. When a pitch appears in both, the highest-velocity note is emitted with a merged (hulled) MPE condition. Pitches unique to one input pass through unchanged. **Pad/Truncate** length mode available. *(Inputs: 2, Outputs: 1)*
- **Xor Node**: Outputs notes whose pitch appears in **exactly one** input — pitches present in both are cancelled. Surviving notes pass through with their original MPE condition. **Pad/Truncate** length mode available. *(Inputs: 2, Outputs: 1)*
- **Zip Node**: Zips two parallel sequences into a single sequence of chords. When the same pitch arrives on both inputs with compatible MPE conditions (e.g. the two outputs of an MPE Filter node re-joined), the conditions are automatically merged into their union range — so the note fires whenever either condition would have passed. *(Inputs: 2, Outputs: 1)*
- **Unzip Node**: Unzips a sequence of chords into two separate mono lines (lowest to Port 0, highest to Port 1). *(Inputs: 1, Outputs: 2)*

## Pitch & Range Manipulation

- **Transpose Node `(Macros)`**: Shifts the entire sequence chromatically by +/- N semitones. *(Inputs: 1, Outputs: 1)*
- **Octave Transpose Node `(Macros)`**: Shifts the sequence by +/- N full octaves. *(Inputs: 1, Outputs: 1)*
- **Quantizer Node `(Macros)`**: Constrains every note to a named scale and root key. Two modes are available: **Filter** drops out-of-scale notes (optionally inserting a rest to preserve rhythmic alignment), and **Snap** moves each note to the nearest in-scale step, breaking ties upward. The scale list, snap mode, and rest-on-drop toggle are macro-bindable.

  Expand the node's unfolded panel to access the **Scale Editor**: an interactive step grid showing one cell per step of the active tuning. Cell brightness indicates harmonic consonance relative to the root (brighter = closer to a low-integer harmonic). Click any cell to toggle that step in or out of the scale, then hit **Save as...** to store the result as a named user scale. The compact panel provides a scale dropdown (populated from the built-in library plus your saved scales) and a root note selector.

  The node is **tuning-aware**: when a microtonal tuning is loaded, the step grid automatically resizes to match the tuning's step count, quantization snaps within the new step space, and the scale dropdown switches to show only scales authored for that tuning. Switching tunings updates all Quantizer nodes in the patch live. *(Inputs: 1, Outputs: 1)*
- **Fold Node `(Macros)`**: Condenses the sequence timeline by folding multiple steps into single chords, either in chunks or via a rolling window. *(Inputs: 1, Outputs: 1)*
- **Unfold Node `(Macros)`**: Extracts chords into sequential linear arpeggios, with controls for ascending/descending sorting, width limits, and note dropping priorities. *(Inputs: 1, Outputs: 1)*

## Routing & Logic

- **Split Node**: Acts as a signal router branching the single sequence into two based on logic (count, percentage, range). *(Inputs: 1, Outputs: 2)*
- **Route Node `(Macros)`**: A switchable track-changer; sends the entire input sequence exclusively to Output 0 OR Output 1 based on an automatable control value. *(Inputs: 1, Outputs: 2)*
- **Select Node `(Macros)`**: An A/B input selector. Allows Input 0 OR Input 1 to pass through to the output, based on an automatable control value. *(Inputs: 2, Outputs: 1)*
- **Switch Node `(Macros)`**: A master "Mute/Unmute" gate for the entire sequence path. *(Inputs: 1, Outputs: 1)*

## Filtering

These nodes split a sequence into two streams by testing a condition. Output 0 (`≥`) carries notes that pass; Output 1 (`<`) carries notes that fail. Rests (empty steps) are preserved on both outputs, keeping downstream sequences in sync.

Conditions can be chained: connecting two MPE Filter nodes in series narrows the active range on each pass. If two outputs with adjacent or overlapping conditions are later re-merged via a Zip node, the conditions are automatically unioned back to a single passthrough range.

- **MPE Filter Node**: Splits notes at playback time based on the live MPE value of the source note. Select which axis to test — **Bend** (X, bipolar ±100%), **Timbre** (Y, 0–100%), or **Pressure** (Z, 0–100%) — and set a threshold. Because the test happens at playback time, the split responds to MPE expression that changes while notes are held, not just the value at note-on. *(Inputs: 1, Outputs: 2)*
- **Velocity Filter Node**: Splits notes at graph-process time based on note-on velocity. Notes with velocity `≥` threshold go to Output 0; notes `<` threshold go to Output 1. Unlike the MPE Filter, this split is resolved when the graph computes — the threshold is compared against the fixed velocity value stored on each note. *(Inputs: 1, Outputs: 2)*
