# MPE & MIDI Configuration

Arps Euclidya supports both standard MIDI and MPE (Multidimensional Polyphonic Expression). Correct configuration in your DAW and within the plugin's MIDI input module is essential for proper expression mapping.

## Non-MPE (Standard MIDI)

For typical MIDI controllers and sequences:

1. **MIDI In Node**: Select the desired MIDI channel (1-16). Set to **0** to listen to all incoming channels.
2. **MPE Toggle**: Ensure MPE is **Disabled** on the MIDI Input module.
3. **DAW Routing**:
    - Many DAWs (like Bitwig) default to **All -> 1**, which flattens all incoming MIDI to Channel 1. This is perfectly fine for standard MIDI use.
    - If you prefer to keep separate inputs on separate channels (e.g. for multiple instruments), use **All -> Same**.

## MPE (Multidimensional Polyphonic Expression)

For controllers like the LinnStrument, Roli Seaboard, or Erae Touch:

1. **MPE Toggle**: MPE must be **Enabled** on the MIDI Input module.
2. **Channel Selection**: In MPE mode, the channel selector is ignored as the plugin automatically manages the MPE zone (typically Master Ch 1, Members 2-16).
3. **DAW Routing (Crucial)**:
    - You **must** set your DAW track to **All -> Same**.
    - Using **All -> 1** will flatten the MPE data into a single channel, breaking per-note expression and causing "pressure bleeding" across all held notes.

> [!TIP]
> If you notice that pressure on one key is affecting all other held notes, check your DAW's MIDI channel mapping. It is likely flattening your MPE data to Channel 1.

## Using MPE Expression in the Graph

Once MPE is enabled, the live per-note expression values (Bend, Timbre, Pressure) are available throughout the graph at playback time. The **MPE Filter Node** lets you branch sequence paths based on these values:

- Choose an axis — **Bend** (X, ±100%), **Timbre** (Y, 0–100%), or **Pressure** (Z, 0–100%) — and set a threshold.
- Output 0 (`≥`) fires notes whose live expression is at or above the threshold; Output 1 (`<`) fires those below.
- Because the test is evaluated at the moment the note would play (not when the graph last computed), the routing responds dynamically to expression changes while notes are held.

Multiple MPE Filter nodes can be chained to create sub-ranges. If you want notes to fire regardless of expression, use a **Zip Node** to re-merge the two outputs — when the same pitch arrives on both inputs with adjacent conditions, the Zip node automatically unions the ranges back to a full passthrough.
