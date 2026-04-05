# The Euclidean Engine

The **MidiOutNode** is arguably the most important module in Arps Euclidya. It acts not just as an endpoint, but as a decoupled clock and rhythm translator.

Traditional arpeggiators couple triggers and pitches: "Play step 3 on the 3rd 16th note." The Euclidean Engine explicitly separates these two concepts. It consists of two entirely decoupled arrays defining the "Pattern" and the "Rhythm".

## Core Layers

### 1. The Rhythm Layer (The "When")

This layer maps directly onto the DAW's clock grid.
It answers the question: *Will the plugin trigger a note right now, or will it output silence?*

- **Rhythm Steps**: The total length of the cycle in clock divisions.
- **Rhythm Beats**: How many active "triggers" are evenly distributed across those steps.
- **Rhythm Offset**: Rotates the trigger pattern chronologically (Phase rotation).

### 2. The Pattern Layer (The "What")

This layer controls sequence progression.
When a "Trigger" occurs in the Rhythm Layer, this Pattern Layer asks: *Which note from the sequence should I play?*

- **Pattern Steps**: The length of the melodic progression cycle before repeating.
- **Pattern Beats**: The density of "Played Notes" versus "Skipped Notes" in the input sequence.
- **Pattern Offset**: Rotates the melodic pattern phase.

*By using non-matching lengths between Rhythm and Pattern arrays, incredibly complex polymetric loops and evolving melodic syncopations naturally emerge.*

## Global Timings & Sync Modes

The engine provides global time divisions (1/4, 1/8, 1/16, triplets, etc.) but dictates phase and reset behavior through **SyncModes**. You can change these on the MidiOutNode visual UI.

- **Synchronized Mode (Default)**: Your trigger timeline is snapped to the DAW grid, but the sequence advances incrementally.
- **Gestural Mode**: Ignores the DAW playhead completely. Whenever you strike the first key, the clock timeline snaps to that exact moment (with relative speed remaining tied to the DAW BPM). This allows for completely un-quantized, human timing and triplet variations.
- **Deterministic Mode**: Strictly ties both the Rhythm index and the Pattern index absolute lengths directly to the absolute DAW playhead position. Even if you press a key halfway through a bar, it will start playing exactly the note in the sequence that belongs to the middle of that bar.

## Expression and Velocity

Every `MidiOutNode` provides internal scaling for incoming MPE dimensions:

- **Pressure -> Velocity**: Maps MPE channel pressure natively to note velocity output.
- **Timbre -> Velocity**: Maps MPE CC74 (Timbre) natively to note velocity output.
These scaling factors can be inverted, added dynamically, and exposed via Macros for real-time live manipulation.

## Humanize

To add organic movement to your sequences, the engine features built-in **Humanize** capabilities:

- Independent variations for Timing (micro-shifts off the rigid grid).
- Velocity randomization.
- Gate Length fluctuations, keeping repetitive arpeggios sounding natural and evolving.

## Multi-Channel Output

Each `MidiOutNode` instance can be assigned to a specific MIDI channel, allowing you to route different branches of your patch to entirely different software instruments or hardware synths from a single Arps Euclidya instance.
