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
