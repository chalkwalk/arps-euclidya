# User Interface Guide

Arps Euclidya provides a fluid, hardware-accelerated interface designed for complex patching and clear visual feedback. The UI is built to keep you in the "flow state", ensuring you always understand how your MIDI data is routing through the system.

## The Interface Layout

When you open Arps Euclidya, the interface is divided into three primary functional areas:

### 1. The Main Canvas

This is your primary workspace where you build and connect your node networks.

- **Infinite Space**: The canvas expands infinitely in all directions as you add nodes, so you never run out of room for complex patches.
- **Fluid Navigation**: You can pan smoothly by clicking and dragging on any empty background space. Use `Ctrl + Scroll` (or `Cmd + Scroll` on macOS) to zoom in and out.
- **Grid-Centric Layout**: To keep your patches organized, nodes automatically snap to a defined grid. When zoomed in, a subtle background grid appears to help you align your modules.
- **Flight Path Preview**: When dragging nodes or cables, a neon-ribbon path visibly connects your cursor to the resolved snap destination, providing clear, real-time feedback on routing boundaries and placements.

### Keyboard Shortcuts

Enhance your workflow with these essential shortcuts:

| Shortcut | Action |
| :--- | :--- |
| `Delete` / `Backspace` | Remove the selected node. |
| `Ctrl + D` | **Duplicate** the selected node (clones it to the nearest free spot). |
| `Space` | **Toggle Transport** (Play/Stop) when in Standalone mode. |
| `Esc` | Cancel current cable drag or node drag. |
| `Ctrl + Scroll` | Zoom In/Out. |

### Context Menus (Right-Click)

Access rapid actions without moving your cursor to the sidebar:

- **Canvas Background**: Right-click any empty space to instantly **Insert a Module** at that position.
- **Module Header**: Right-click the title bar of any node to **Replace** it with a different module type while preserving compatible connections, or use the **Bypass** toggle on the header to temporarily disable its effects.
- **Node Knobs**: Right-click any parameter knob to **Map to Macro**.

### 2. Module Library (Sidebar)

Located in a collapsible sidebar, this is your toolbox. It contains every node available in Arps Euclidya, categorized by function.

- You can browse nodes by category or use the rapid "fuzzy-search" bar at the top to instantly find the module you need. The module list is alphabetically sorted for quick visual scanning.

### 3. Macro Control Panel

Depending on your DAW and window configuration, this panel surfaces the 32 global "Macro" knobs.

- Because the node graph is deep inside the plugin, your DAW cannot naturally see the internal parameters. These 32 macros act as the "bridge" between your DAW's automation system and your custom patch.

## Reading the Graph

Understanding your patch visually is critical:

- **Nodes**: Each module is represented as a block on the canvas. Inputs are always located on the left edge, and outputs are on the right edge.
- **Patch Cables**: Connections between nodes glow vividly over the dark background, making signal flow instantly obvious even in dense patches.
- **Visual Cues**: Active elements, selected nodes, and signal flow warnings use high-contrast neon colors so you can tell at a glance what is happening in your patch.
