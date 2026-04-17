# User Experience & Workflows

Arps Euclidya is designed to prioritize fast, fluid patching over tedious menu-diving. This section covers how to interact with the graph and how to prepare your patches for live performance using Macros.

## Fast Patching Gestures

To keep you in the "flow state", the UI offers several advanced patching interactions:

- **Library Search**: Open the Module Library sidebar and use the search bar. Typing a partial name and pressing `Enter` will spawn the topmost result at your mouse cursor.
- **Cable Insertion (Splicing)**: If you drag a new node from the library directly over an *active* patch cable, the wire will highlight. Dropping the node will "splice" it into that connection, routing the previous output into the new node, and the new node's output to the original destination.
- **Edge-Drop Auto-Connect**: Dragging and dropping a node onto the left edge of an existing node connects its outputs to the existing node's inputs. Dropping on the right edge does the reverse.
- **Smart Disconnect**: You can pull cables off of ports to break a connection, or delete nodes by selecting them and pressing `Delete`/`Backspace`.

## Advanced Workflows

These interactions are designed to support several "Common Workflows":

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

The plugin exposes exactly **32 global Macro parameters** to your DAW (e.g., Ableton, Bitwig, Reaper). The Macro panel runs along the top of the plugin window. Each macro is assigned a distinct color that tracks through every visual indicator in the system, so you always know at a glance which macro is responsible for which movement.

### Creating a Binding (Shift+Drag)

The primary binding gesture is **shift+drag on any node knob**:

1. **Select a macro** by clicking its palette slot — it will glow in its color and a faint ring appears on every unbound parameter knob in the graph, indicating they are ready to be claimed.
2. **Shift+drag** any parameter knob. The binding is created immediately and the drag distance sets its **intensity** — how strongly the macro's value contributes to that parameter. Drag further up or down from where you started to increase or decrease intensity; the colored arc around the knob grows in real time.
3. **Release** — the binding is set. Moving the macro knob in your DAW now modulates the parameter by the specified intensity on top of its local (hand-set) value.

> **Shortcut**: If you shift+drag a knob without selecting any macro first, Arps Euclidya automatically selects the next free macro and creates the binding in one gesture.

### Reading the Visual Feedback

Each bound parameter displays several layers of visual information:

| Indicator | Meaning |
| :--- | :--- |
| **Colored arc** (outside the knob) | Binding exists; arc length = intensity. Multiple bindings stack as concentric rings in each macro's color. |
| **White bridge arc + hollow ring** (on the knob face) | The *effective* value after all macro contributions — shows where the parameter actually lands right now. Moves in real time. |
| **Colored border** (on toggle buttons) | The button is bound to a macro; border color matches the macro. |
| **Intensity bar** (bottom of toggle button) | Width = total binding intensity. |
| **Small dot** (right edge of toggle button) | Macro is currently overriding the button's local state. |

### Hover Interactions

- **Hover a node knob or button** → the macro palette slots it is bound to light up with a white ring, so you can instantly identify which macros control that parameter.
- **Hover a macro palette slot** → all controls on the canvas that are bound to that macro light up with a white ring, giving you a complete map of that macro's reach across your patch.

### Bipolar Macros

By default macros are **unipolar** (0 → max contribution). **Double-click** any macro palette slot to toggle it to **bipolar** mode:

- A `±` indicator and a centre tick mark appear on the macro knob.
- In bipolar mode the knob's centre position (12 o'clock) contributes nothing; turning clockwise adds positively, counter-clockwise adds negatively.
- The binding arc shifts to a symmetric shape centered on the knob's midpoint.

Macros 1–16 default to unipolar, macros 17–32 default to bipolar.

### Removing Bindings

- **Right-click** any bound parameter knob → a menu lists each binding as "Remove: Macro N". Click one to remove it.
- **Right-click** a macro palette slot → "Clear all bindings for Macro N" removes every binding that macro has across the entire patch at once.

### Binding Toggle Buttons

Route, Select, Switch, and Quantizer nodes expose toggle buttons that can also be macro-bound. The gesture is a **shift+click** on the button instead of a drag. Each click with the macro selected toggles the binding on or off.

## Interactive Feedback

The graph isn't just static logic; it provides visual feedback:

- **Signal Visualization**: Hovering over any patch cable displays a tooltip showing the current sequence status, including length and active steps. The cable visualization uses dynamic color coding to indicate sequence state and will turn orange or red to warn you if a sequence grows dangerously long (e.g., from an uncontrolled Multiply node).
- **Interactive UI Elements**: Some nodes, notably the `MidiOutNode` (The Euclidean Engine), feature interactive components. You can drag the circular Euclidean rings directly in the UI to shift offsets or toggle sequence steps manually.
