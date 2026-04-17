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

The engine provides global time divisions (1/4, 1/8, 1/16, triplets, etc.) and controls how playback phase relates to the DAW timeline through four **Sync Modes**. In all modes, tempo always tracks the DAW BPM. What differs is *when* the first note plays and *how* subsequent notes stay in phase.

### Gestural *(free phase)*

The first note fires the instant you press a key, regardless of where the DAW playhead is. All subsequent notes fall at strict division intervals measured from that key-press moment — so the sequence is always in perfect musical time, just not necessarily locked to the DAW's bar lines.

Use this when you want expressive, human-timed playing. Releasing and re-pressing the keys starts the phase over from wherever you press.

### Synchronized *(grid-locked phase, default)*

When you press a key, the engine waits for the next clock division boundary on the DAW grid before firing the first note — whether you pressed just before or just after a beat. Subsequent notes then follow the grid from that point.

Releasing the keys resets the sequence and rhythm positions, so pressing again always replays the same pattern from the top, just delayed to the next beat. The result sounds identical to Gestural but stays locked to the bar.

### Deterministic *(absolute phase)*

Like Synchronized, the first note waits for the next clock division boundary. The key difference is that the sequence and rhythm positions are derived from the *absolute DAW playhead position*, not from a beat-count since you pressed the key.

This means the pattern is fully reproducible: pressing at bar 1 or bar 32 always plays exactly the notes that belong to that point in the absolute timeline. Jogging or looping in the DAW causes the pattern to jump to the corresponding position — useful for arrangement work where you need the same phrase to be replayable at the same point every time.

### Forgiving *(grace-window hybrid)*

Designed for live performance where you are trying to hit a beat precisely but occasionally land fractionally late. The engine applies a short grace window (1/8 of the current clock division) after each beat:

- **Within the grace window** (pressed slightly after the beat): the first note fires immediately, and the engine gradually corrects the timing phase over the next few ticks — each subsequent note arrives slightly earlier than the last until the sequence is back in phase with the grid.
- **Outside the grace window** (pressed well before or after a beat): the engine behaves like Synchronized and waits for the next beat before firing.

The correction is subtle and exponential: within a handful of notes you are back in sync, with no jarring tempo jump.

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
