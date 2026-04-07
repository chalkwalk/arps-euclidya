# Introduction to Arps Euclidya

Traditional arpeggiators are often limited to linear patterns and fixed rhythmic scales. **Arps Euclidya** breaks this mold. It is a highly performable and flexible modular MIDI environment that decouples the "what" (melody) from the "when" (rhythm).

## Core Design Goals

The primary goal of Arps Euclidya is to bridge the gap between deep, offline patch design and expressive, real-time performance. It allows you to:

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
   A single Arps Euclidya instance can handle multiple incoming MIDI streams and direct its output to multiple distinct MIDI targets, functioning as the central nervous system of a complex MIDI routing setup.
4. **Chord Support**
   Unlike basic arpeggiators that simply smash chords into single-note lines, Euclidya provides specialized combinatorial nodes. You can extract top or bottom notes, exhaustively span N-note combinations, or intelligently unzip chords into separate polyphonic sequences.
5. **Euclidean Rhythm and Melody**
   Central to the plugin is its unique approach to rhythm. By employing decoupled Euclidean arrays for *Pattern* (note play vs. skip) and *Rhythm* (clock trigger vs. rest), the engine generates evolving polyrhythms that remain harmonically tethered to your input.
