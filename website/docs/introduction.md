# Introduction to Arps Euclidya

Traditional arpeggiators are often limited to linear patterns and fixed rhythmic scales. **Arps Euclidya** takes a different approach. It is a highly performable and flexible modular MIDI environment that decouples the "what" (melody) from the "when" (rhythm).

## Core Design Goals

The primary goal of Arps Euclidya is to connect deep, offline patch design and expressive, real-time performance. It allows you to:

- **Design Patches Offline**: Build intricate di-graphs of logic, routing, and algorithmic generation without the pressure of live playback.
- **Perform in Real-Time**: Map any parameter to global **Macro controls**, exposing your complex logic to standard DAW automation or external MIDI controllers.
- **Drive with Multiple Controllers**: Send MIDI from various hardware keyboards or sequencers into the patch, and route the generated output to entirely different hardware synths or plugins simultaneously.

## The "Big Five" Features

The foundation of Arps Euclidya rests upon five critical pillars:

1. **Modular Design**
   There is no fixed signal path. You can route any node to any other (acyclically) to create custom generative systems, logic-based switches, or parallel processing chains. The canvas is infinite, allowing for incredibly deep architectures.
2. **MPE Support (MIDI Polyphonic Expression)**
   Expression is treated as a first-class citizen. Per-note dimensions (Pitch Bend, Timbre/CC74, and Pressure) get captured and remain associated with the relevant notes at output allowing for intuitive and expressive control.
3. **Multi Input and Output Per Patch**
   A single Arps Euclidya instance can handle multiple incoming MIDI streams and direct its output to multiple distinct MIDI targets, functioning as the main hub of a complex MIDI routing setup.
4. **Chord Support**
   Unlike basic arpeggiators that simply smash chords into single-note lines, Euclidya provides specialized combinatorial nodes. You can extract top or bottom notes, exhaustively span N-note combinations, or intelligently unzip chords into separate polyphonic sequences.
5. **Euclidean Rhythm and Melody**
   Central to the plugin is its unique approach to rhythm. By employing decoupled Euclidean arrays for *Pattern* (note play vs. skip) and *Rhythm* (clock trigger vs. rest), the engine generates evolving polyrhythms that remain harmonically tethered to your input.
6. **Microtonal & Tuning-Aware Quantization**
   Load any Scala/KBM tuning file and every part of the system adapts: pitch-bend output from the MIDI Out node tracks each note's deviation from equal temperament, and the Quantizer node's step grid resizes to the tuning's step count. A built-in harmonic-proximity scorer shades each step by consonance, making it easy to author and save scales in any tuning system directly from the panel.
