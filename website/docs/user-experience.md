# User Experience & Workflows

Arps Euclidya is designed to prioritize fast, fluid patching over tedious menu-diving. This section covers how to interact with the graph and how to prepare your patches for live performance using Macros.

## Fast Patching Gestures

To keep you in the "flow state", the UI offers several advanced patching interactions:

- **Library Search**: Open the Module Library sidebar and use the fuzzy-search bar. Simply typing a partial name and pressing `Enter` will instantly spawn the topmost result at your mouse cursor.
- **Cable Insertion (Splicing)**: If you drag a new node from the library directly over an *active* patch cable, the wire will highlight. Dropping the node will automatically "splice" it into that connection, routing the previous output into the new node, and the new node's output to the original destination.
- **Edge-Drop Auto-Connect**: Dragging and dropping a node onto the left edge of an existing node automatically connects its outputs to the existing node's inputs. Dropping on the right edge does the reverse.
- **Smart Disconnect**: You can pull cables off of ports to break a connection, or delete nodes by selecting them and pressing `Delete`/`Backspace`.

## Advanced Workflows (CUJs)

These interactions are designed to support several "Critical User Journeys":

### 1. Rapid Signal Prototyping

When you have a specific idea for a sound (e.g., "I need a rhythmic quantizer feeding into a fold node"), you don't need to touch the sidebar.

- **Action**: Right-click the background to insert the first node.
- **Action**: Use **Ctrl + D** to clone a node you already have configured.
- **Action**: Drag the new node onto an existing cable to **Splice** it in instantly.

### 2. Iterative Sound Design

Often you have a complex patch but want to audition alternative logic without re-wiring everything.

- **Action**: Right-click the header of a module you want to change.
- **Action**: Select **"Replace with..."** and choose a new module type.
- **Benefit**: The system automatically transfers compatible input and output connections to the new node, keeping your signal path intact while you swap the processing logic.

## The Macro System (DAW Automation)

One of the most important concepts in Arps Euclidya is the **Macro System**. Because nodes exist in a dynamic graph inside the plugin, the DAW itself cannot "see" individual node parameters.

To solve this and enable live performance automation:

1. **Global Macros**: The plugin exposes exactly 32 "Macro" parameters to your DAW (e.g., Ableton, Bitwig, Reaper).
2. **Mapping**: Inside Arps Euclidya, you can right-click almost any rotary knob on a node and assign it to one of these 32 Macros.
3. **Control**: Once mapped, turning the Macro knob in your DAW (or twisting a MIDI controller mapped to that DAW parameter) will instantly control the node's parameter deep within the patch. All mapping assignments and removals are fully supported by the **Undo/Redo system**.
4. **Performance**: This allows you to build a massive, complex patch "offline," decide which 4 or 5 parameters represent the "performance surface" of that patch, map them to Macros, and then perform with them live using hardware controllers.

## Interactive Feedback

The graph isn't just static logic; it provides visual feedback:

- **Signal Visualization**: Hovering over any patch cable displays a tooltip showing the current sequence status, including length and active steps. The cable visualization uses dynamic color coding to indicate sequence state and will turn orange or red to warn you if a sequence grows dangerously long (e.g., from an uncontrolled Multiply node).
- **Interactive UI Elements**: Some nodes, notably the `MidiOutNode` (The Euclidean Engine), feature interactive components. You can drag the circular Euclidean rings directly in the UI to shift offsets or toggle sequence steps manually.
