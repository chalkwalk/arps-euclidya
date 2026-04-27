# User Interface Guide

Arps Euclidya provides a hardware-accelerated interface designed for complex patching and clear visual feedback. The UI is built to keep you in the "flow state", ensuring you always understand how your MIDI data is routing through the system.

## The Interface Layout

When you open Arps Euclidya, the interface is divided into three primary functional areas:

### 1. The Main Canvas

This is your primary workspace where you build and connect your node networks.

- **Infinite Space**: The canvas expands infinitely in all directions as you add nodes, so you never run out of room for complex patches.
- **Fluid Navigation**: You can pan smoothly by clicking and dragging on any empty background space. Use `Ctrl + Scroll` (or `Cmd + Scroll` on macOS) to zoom in and out.
- **Grid-Centric Layout**: To keep your patches organized, nodes automatically snap to a defined grid. When zoomed in, a subtle background grid appears to help you align your modules.
- **Flight Path Preview**: When dragging nodes or cables, a clear path visibly connects your cursor to the resolved snap destination, providing clear, real-time feedback on routing boundaries and placements.
- **Unfolding Panels**: Node panels can be unfolded to reveal advanced parameters. These unfolded panels support expansion in all directions (up, down, left, right) depending on the node's layout. For example, unfolding the Quantizer reveals an interactive step-grid scale editor.
- **Tabbing Support**: Inside expanded node panels, tabbing structures allow deep configuration elements to be grouped neatly without taking up excessive screen real estate.

### Keyboard Shortcuts

Enhance your workflow with these essential shortcuts:

| Shortcut | Action |
| :--- | :--- |
| `Delete` / `Backspace` | Remove the selected node. |
| `Ctrl + D` | **Duplicate** the selected node (clones it to the nearest free spot). |
| `Space` | **Toggle Transport** (Play/Stop) when in Standalone mode. |
| `Esc` | Cancel current cable drag or node drag. |
| `Ctrl + Scroll` | Zoom In/Out. |

### Parameter Knob Gestures

All rotary knobs in node panels support the following mouse gestures:

| Gesture | Action |
| :--- | :--- |
| **Drag** | Adjust value at normal speed. |
| **Double-click** | Reset to default value. |
| **Shift + drag** | Fine adjustment — approximately ¼ speed, for precise edits. Shift can be pressed or released at any point during a drag to toggle fine mode on or off. |
| **Ctrl + drag** (Cmd on macOS) | Snap to coarse increments — jumps through ~20 evenly-spaced steps across the range. Ctrl can be pressed or released mid-drag to toggle snap on or off. |
| **Alt + drag** | Macro binding — sets or adjusts the intensity of a binding to the currently selected macro (see [The Macro System](user-experience.md#the-macro-system-daw-automation)). |
| **Alt + double-click** | Open MIDI Learn for this parameter. |
| **Right-click** | Open context menu (remove bindings, MIDI learn). |

### Context Menus (Right-Click)

Access rapid actions without moving your cursor to the sidebar:

- **Canvas Background**: Right-click any empty space to instantly **Insert a Module** at that position.
- **Module Header**: Right-click the title bar of any node to **Replace** it with a different module type while preserving compatible connections, or use the **Bypass** toggle on the header to temporarily disable its effects.
- **Node Knobs / Buttons**: Right-click any bound parameter to see a "Remove: Macro N" option for each existing binding (fully undoable), or use the **MIDI Learn** feature to map a hardware CC to the control natively.
- **Macro Palette Slot**: Right-click any macro in the Macro Panel to **clear all bindings** that macro holds across the entire patch, or map an external CC directly via MIDI learn.

### 2. Module Library (Sidebar)

Located in a collapsible sidebar, this is your toolbox. It contains every node available in Arps Euclidya, organized into categories: **I/O**, **Modulation**, **Generators**, **Pattern**, **Combinatorial**, **Pitch & Range**, **Routing**, **Filter**, and **Utility**.

- Browse nodes by category or use the search bar to switch to a flat filtered list. The search supports **multiple space-separated tokens** (AND semantics) and matches against the node name, its category, and any associated tags — so `rhythm gen` finds generator-category nodes tagged with `rhythm`, and `mpe filter` finds the MPE Filter node. Pressing `Enter` when exactly one result is shown adds that node at the current canvas position.

### 3. Macro Control Panel

The panel along the top of the plugin window surfaces the 32 global "Macro" knobs. Each macro has a distinct color that propagates throughout the UI.

- **Selecting**: Click a macro slot to select it (it glows in its color). Click it again to deselect. Only one macro can be selected at a time.
- **Binding**: With a macro selected, alt+drag any node parameter knob to create a binding. The drag amount sets the binding's intensity. See [The Macro System](user-experience.md#the-macro-system-daw-automation) for the full workflow.
- **Bipolar toggle**: Double-click any macro slot to switch between unipolar and bipolar modulation.
- **Hover feedback**: Hovering over a macro slot highlights every canvas control that is currently bound to it.
- **Clearing**: Right-click a macro slot to clear all of its bindings across the entire patch.
- **CC Mapping**: Right-click the macro slot to enable MIDI Learn and map your external hardware controller's CC messages directly.
- Because the node graph is deep inside the plugin, your DAW cannot naturally see the internal parameters. These 32 macros act as the "bridge" between your DAW's automation system and your custom patch.

## Reading the Graph

Understanding your patch visually is critical:

- **Nodes**: Each module is represented as a block on the canvas. Inputs are always located on the left edge, and outputs are on the right edge.
- **Patch Cables**: Connections between nodes glow vividly over the dark background. Cable colour indicates data type: **gold** for note sequences, **violet** for CC sequences, and **grey/white** for agnostic connections that carry whichever type was first routed through them. Dragging a note cable onto a CC input (or vice versa) is rejected with a brief red flash.
- **Visual Cues**: Active elements, selected nodes, and signal flow warnings use high-contrast colors so you can tell at a glance what is happening in your patch.
