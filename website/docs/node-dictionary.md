# Node Dictionary

The Arps Euclidya environment offers 28+ specialized nodes, broken down into functional categories. Nodes marked `(Macros)` expose one or more parameters to the macro system — select a macro in the Macro Panel, then shift+drag the parameter knob (or shift+click a toggle button) to create a binding. Additionally, all nodes feature a **Bypass toggle** in their UI header to easily A/B test processing behavior without altering patch cables.

## Core I/O & Generation

- **Midi In Node**: The entry point for live performance. Captures notes and MPE data from the DAW or the on-screen keyboard. *(No Inputs, Outputs: 1)*
- **Midi Out Node `(Macros)`**: The terminal destination and home of the Euclidean Engine. Converts the sequenced note blocks back into real-time MIDI events for the host. Features robust "Humanize" parameters (timing, velocity, gate variations) and per-node MIDI channel output routing for independent instrument control. *(Inputs: 1, No Outputs)*
- **Sequence Node `(Macros)`**: A 16-step "piano roll" generator. Allows users to draw static patterns that play continuously, independent of held keys. *(No Inputs, Outputs: 1)*
- **All Notes Node**: A utility node that steadily outputs every possible MIDI note (C-2 to G8). Useful as an input signal for test routing or scale folding. *(No Inputs, Outputs: 1)*
- **Diagnostic Node**: Displays step count, note pitches, velocities, and active MPE conditions (`{X≥50% Z<30%}` style) for each step. Useful for inspecting the effect of upstream filter nodes. *(Inputs: 1, Outputs: 1)*

## Pattern & Order (Directional)

- **Sort Node**: Reorders the incoming sequence by grouping and sorting notes into ascending pitch. Perfect for traditional "Up" arpeggios. *(Inputs: 1, Outputs: 1)*
- **Reverse Node**: Flips the chronological order of the steps. Creates a "Down" arpeggio. *(Inputs: 1, Outputs: 1)*
- **Walk Node `(Macros)`**: Applies a "Brownian motion" logic to the sequence. You can configure the probability of stepping forward, staying on the same step, or stepping backward. *(Inputs: 1, Outputs: 1)*
- **Converge Node**: Merges two input sequences. *(Inputs: 2, Outputs: 1)*
- **Diverge Node**: Splits paths from a single input sequence. *(Inputs: 1, Outputs: 2)*

## Combinatorial & Chord Logic

- **Chord N Node `(Macros)`**: Generates exhaustive N-note combinations from the incoming sequence or held chords. Transforms simple 4-note input chords into deep, evolving tapestries. *(Inputs: 1, Outputs: 1)*
- **Chord Split Node**: Extracts the "Top" note to Output A, and the "Bottom" note to Output B. Excellent for creating separate basslines and melodies from the same chord pad. *(Inputs: 1, Outputs: 2)*
- **Octave Stack Node `(Macros)`**: Duplicates the sequence pattern and stacks it at user-defined octave offsets, creating thick textures. Ensures no duplicate pitches are clustered. *(Inputs: 1, Outputs: 1)*
- **Multiply Node `(Macros)`**: Stretches the sequence by repeating each step N times consecutively. *(Inputs: 1, Outputs: 1)*
- **Concatenate Node**: Appends Sequence B to the end of Sequence A. Often used with a Sort and Reverse node to build a classic "Up/Down" arpeggiator. *(Inputs: 2, Outputs: 1)*
- **Interleave Node**: Alternates steps from two inputs: A[0], B[0], A[1], B[1], … If the inputs differ in length, the shorter is padded with rests. Output length = 2 × max(len(A), len(B)). *(Inputs: 2, Outputs: 1)*
- **Zip Node**: Zips two parallel sequences into a single sequence of chords. When the same pitch arrives on both inputs with compatible MPE conditions (e.g. the two outputs of an MPE Filter node re-joined), the conditions are automatically merged into their union range — so the note fires whenever either condition would have passed. *(Inputs: 2, Outputs: 1)*
- **Unzip Node**: Unzips a sequence of chords into two separate mono lines (lowest to Port 0, highest to Port 1). *(Inputs: 1, Outputs: 2)*

## Pitch & Range Manipulation

- **Transpose Node `(Macros)`**: Shifts the entire sequence chromatically by +/- N semitones. *(Inputs: 1, Outputs: 1)*
- **Octave Transpose Node `(Macros)`**: Shifts the sequence by +/- N full octaves. *(Inputs: 1, Outputs: 1)*
- **Quantizer Node `(Macros)`**: Forces every note in the sequence onto a specific musical scale and key. Scale mode and "rest on drop" behavior are macro-bindable. *(Inputs: 1, Outputs: 1)*
- **Fold Node `(Macros)`**: Constrains sequences that drift out of bounds back into a target octave range by "folding" or wrapping the overflowing pitches. *(Inputs: 1, Outputs: 1)*
- **Unfold Node**: Expands the pitch range iteratively by pushing subsequent steps into higher or lower sub-octaves. *(Inputs: 1, Outputs: 1)*

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
