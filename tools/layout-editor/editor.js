// ============================================================
// JUCE Layout Constants (mirrors LayoutConstants.h + NodeBlock.h)
// ============================================================
const GRID_PITCH = 100;      // Layout::GridPitch
const TRAMLINE_MARGIN = 10;  // Layout::TramlineMargin
const HEADER_HEIGHT = 28;    // NodeBlock::HEADER_HEIGHT
const PORT_MARGIN = 14;      // NodeBlock::PORT_MARGIN
const PORT_RADIUS = 8;       // NodeBlock::PORT_RADIUS
const PORT_SPACING = 24;     // NodeBlock::PORT_SPACING
const BOTTOM_PAD = 4;
const SUBS_PER_UNIT = 20;  // NodeBlock::resized SUBS_PER_UNIT

// Display scale factor (so the preview isn't tiny; 3 = 300%)
let SCALE = 3;

// ============================================================
// Colors (from ArpsLookAndFeel + NodeBlock paint)
// ============================================================
const COLOR_BG_DARK = '#0b1016';  // getBackgroundCharcoal
const COLOR_BG_NODE = '#121a24';  // getForegroundSlate
const COLOR_NEON = '#0df0e3';     // getNeonColor
const COLOR_NEON_DIM = 'rgba(13, 240, 227, 0.3)';
const COLOR_TRACK_DARK = '#333333';  // rotarySliderOutlineColourId
const COLOR_PORT_IN = '#44cc44';
const COLOR_PORT_IN_BD = '#228822';
const COLOR_PORT_OUT = '#4499ff';
const COLOR_PORT_OUT_BD = '#2266aa';
const COLOR_TEXT = '#ffffff';
const COLOR_LABEL = '#cccccc';
const COLOR_COMBO_BG = '#121a24';
const COLOR_COMBO_BORDER = 'rgba(13, 240, 227, 0.3)';

// JUCE rotary defaults (LookAndFeel_V4)
const ROTARY_START = Math.PI * 1.2;  // ~7 o'clock (radians from top)
const ROTARY_END = Math.PI * 2.8;    // ~5 o'clock

// ============================================================
// App State
// ============================================================
let dirHandle = null;
let nodeFiles = new Map();
let currentNodeKey = null;
let currentData = null;
let selectedElementIndex = -1;

const btnOpen = document.getElementById('btn-open-dir');
const selNode = document.getElementById('sel-node');
const btnSave = document.getElementById('btn-save');
const statusMsg = document.getElementById('status-message');
const wrapper = document.getElementById('canvas-wrapper');
const propsContent = document.getElementById('props-content');
const scaleSlider = document.getElementById('scale-slider');
const scaleDisplay = document.getElementById('scale-display');

// We use a single <canvas> element for the full node preview draw
let previewCanvas = null;
let previewCtx = null;

// DOM overlay layer for interaction (drag/resize handles)
let overlayDiv = null;

function initCanvas() {
  wrapper.innerHTML = '';

  previewCanvas = document.createElement('canvas');
  previewCanvas.id = 'preview-canvas';
  previewCanvas.style.display = 'block';
  previewCanvas.style.cursor = 'crosshair';
  wrapper.appendChild(previewCanvas);
  previewCtx = previewCanvas.getContext('2d');

  // Overlay div sits on top for interaction handles
  overlayDiv = document.createElement('div');
  overlayDiv.id = 'overlay';
  overlayDiv.style.position = 'absolute';
  overlayDiv.style.top = '0';
  overlayDiv.style.left = '0';
  overlayDiv.style.pointerEvents = 'none';
  wrapper.style.position = 'relative';
  wrapper.appendChild(overlayDiv);

  previewCanvas.addEventListener('mousedown', onCanvasMouseDown);
  previewCanvas.addEventListener('mousemove', onCanvasMouseMove);
  document.addEventListener('mousemove', onDocMouseMove);
  document.addEventListener('mouseup', onDocMouseUp);
}

// ============================================================
// File System Logic
// ============================================================
btnOpen.addEventListener('click', async () => {
  try {
    dirHandle = await window.showDirectoryPicker();
    statusMsg.textContent = 'Scanning directory...';
    nodeFiles.clear();
    await scanDirectory(dirHandle, '');
    populateDropdown();
    statusMsg.textContent = `Found ${nodeFiles.size} node JSON files.`;
    selNode.disabled = false;
  } catch (err) {
    console.error(err);
    statusMsg.textContent = 'Failed to open directory.';
  }
});

async function scanDirectory(handle, path) {
  for await (const entry of handle.values()) {
    const fullPath = path ? `${path}/${entry.name}` : entry.name;
    if (entry.kind === 'directory') {
      await scanDirectory(entry, fullPath);
    } else if (entry.kind === 'file' && entry.name.endsWith('.json')) {
      const parts = fullPath.split('/');
      if (parts.length >= 2 &&
          parts[parts.length - 2] === entry.name.replace('.json', '')) {
        nodeFiles.set(fullPath, entry);
      }
    }
  }
}

function populateDropdown() {
  selNode.innerHTML = '<option value="">Select a Node JSON...</option>';
  const sortedKeys = Array.from(nodeFiles.keys()).sort();
  for (const k of sortedKeys) {
    const opt = document.createElement('option');
    opt.value = k;
    opt.textContent = k;
    selNode.appendChild(opt);
  }
}

selNode.addEventListener('change', async (e) => {
  const key = e.target.value;
  if (!key) return;
  try {
    statusMsg.textContent = 'Loading...';
    const fileHandle = nodeFiles.get(key);
    const file = await fileHandle.getFile();
    const text = await file.text();
    currentData = JSON.parse(text);
    currentNodeKey = key;
    selectedElementIndex = -1;

    document.getElementById('node-w').value = currentData.gridWidth || 1;
    document.getElementById('node-h').value = currentData.gridHeight || 1;

    initCanvas();
    renderAll();
    renderProperties();
    statusMsg.textContent = 'Loaded ' + key;
    btnSave.disabled = false;
  } catch (err) {
    console.error(err);
    statusMsg.textContent = 'Error loading file.';
  }
});

btnSave.addEventListener('click', async () => {
  if (!currentData || !currentNodeKey) return;
  try {
    statusMsg.textContent = 'Saving...';
    currentData.elements.forEach(el => {
      el.gridBounds = el.gridBounds.map(v => Math.round(v));
    });
    const jsonStr = JSON.stringify(currentData, null, 4);
    const fileHandle = nodeFiles.get(currentNodeKey);
    const writable = await fileHandle.createWritable();
    await writable.write(jsonStr);
    await writable.close();
    statusMsg.textContent = 'Saved successfully.';
  } catch (err) {
    console.error(err);
    statusMsg.textContent = 'Failed to save.';
  }
});

document.getElementById('node-w').addEventListener('change', (e) => {
  if (currentData) {
    currentData.gridWidth = parseInt(e.target.value);
    renderAll();
  }
});
document.getElementById('node-h').addEventListener('change', (e) => {
  if (currentData) {
    currentData.gridHeight = parseInt(e.target.value);
    renderAll();
  }
});

scaleSlider.addEventListener('input', () => {
  SCALE = parseFloat(scaleSlider.value);
  scaleDisplay.textContent = SCALE.toFixed(1) + '×';
  if (currentData) renderAll();
});

// ============================================================
// Geometry Helpers — exact mirrors of JUCE NodeBlock math
// ============================================================

function nodePixelSize(gw, gh) {
  return {
    w: (gw * GRID_PITCH - TRAMLINE_MARGIN) * SCALE,
    h: (gh * GRID_PITCH - TRAMLINE_MARGIN) * SCALE,
  };
}

function bodyRegion(nodeW, nodeH) {
  // Mirrors NodeBlock::resized():
  //   startX = PORT_MARGIN
  //   startY = HEADER_HEIGHT
  //   bodyWidth  = width  - PORT_MARGIN * 2
  //   bodyHeight = height - HEADER_HEIGHT - BOTTOM_PAD
  return {
    x: PORT_MARGIN * SCALE,
    y: HEADER_HEIGHT * SCALE,
    w: nodeW - PORT_MARGIN * 2 * SCALE,
    h: nodeH - HEADER_HEIGHT * SCALE - BOTTOM_PAD * SCALE,
  };
}

function subGridPitch(bodyW, bodyH, gridW, gridH) {
  const cols = gridW * SUBS_PER_UNIT;
  const rows = gridH * SUBS_PER_UNIT;
  return {sx: bodyW / cols, sy: bodyH / rows};
}

function elementRect(el, body, sx, sy) {
  const b = el.gridBounds;
  return {
    x: body.x + b[0] * sx,
    y: body.y + b[1] * sy,
    w: b[2] * sx,
    h: b[3] * sy,
  };
}

// ============================================================
// Main Render
// ============================================================

function renderAll() {
  if (!currentData || !previewCtx) return;

  const gw = currentData.gridWidth || 1;
  const gh = currentData.gridHeight || 1;
  const {w: nodeW, h: nodeH} = nodePixelSize(gw, gh);

  previewCanvas.width = nodeW;
  previewCanvas.height = nodeH;

  const ctx = previewCtx;
  const body = bodyRegion(nodeW, nodeH);
  const {sx, sy} = subGridPitch(body.w, body.h, gw, gh);

  // --- Node body ---
  drawNodeBody(ctx, nodeW, nodeH);

  // --- Sub-grid in body area ---
  drawSubGrid(ctx, body, gw, gh, sx, sy);

  // --- Elements ---
  currentData.elements.forEach((el, i) => {
    const r = elementRect(el, body, sx, sy);
    const selected = (i === selectedElementIndex);
    drawElement(ctx, el, r, selected);
  });

  // --- Selected element border (on top) ---
  if (selectedElementIndex >= 0 &&
      selectedElementIndex < currentData.elements.length) {
    const el = currentData.elements[selectedElementIndex];
    const r = elementRect(el, body, sx, sy);
    ctx.strokeStyle = COLOR_NEON;
    ctx.lineWidth = 1.5;
    ctx.setLineDash([4, 3]);
    ctx.strokeRect(r.x + 0.5, r.y + 0.5, r.w - 1, r.h - 1);
    ctx.setLineDash([]);
  }

  // --- Update overlay interaction handles ---
  renderOverlay(body, sx, sy);
}

function drawNodeBody(ctx, nodeW, nodeH) {
  const R = 6 * (SCALE / 3);

  // Background
  ctx.fillStyle = COLOR_BG_NODE;
  roundRect(ctx, 0, 0, nodeW, nodeH, R);
  ctx.fill();

  // Neon border
  ctx.strokeStyle = COLOR_NEON_DIM;
  ctx.lineWidth = 1.5;
  roundRect(ctx, 0.5, 0.5, nodeW - 1, nodeH - 1, R);
  ctx.stroke();

  // Header divider line
  ctx.strokeStyle = 'rgba(13, 240, 227, 0.2)';
  ctx.lineWidth = 1;
  ctx.beginPath();
  ctx.moveTo(0, HEADER_HEIGHT * SCALE);
  ctx.lineTo(nodeW, HEADER_HEIGHT * SCALE);
  ctx.stroke();

  // Title text
  const nodeName = currentNodeKey ? currentNodeKey.split('/')
                                        .pop()
                                        .replace('.json', '')
                                        .replace('Node', ' Node') :
                                    'Node';
  ctx.fillStyle = COLOR_TEXT;
  ctx.font = `bold ${Math.max(9, 14 * SCALE / 3)}px "Segoe UI", sans-serif`;
  ctx.textAlign = 'left';
  ctx.textBaseline = 'middle';
  ctx.fillText(nodeName, 8 * SCALE / 3, HEADER_HEIGHT * SCALE / 2);

  // Delete button placeholder (top-right)
  const btnSize = (HEADER_HEIGHT - 6) * SCALE;
  const btnX = nodeW - btnSize - 3 * SCALE / 3;
  const btnY = 3 * SCALE / 3;
  ctx.fillStyle = 'rgba(0,0,0,0.2)';
  roundRect(ctx, btnX, btnY, btnSize, btnSize, 3);
  ctx.fill();
  ctx.fillStyle = 'rgba(255,255,255,0.5)';
  ctx.font = `${Math.max(7, 10 * SCALE / 3)}px sans-serif`;
  ctx.textAlign = 'center';
  ctx.textBaseline = 'middle';
  ctx.fillText('×', btnX + btnSize / 2, btnY + btnSize / 2);

  // Input ports (left edge half-stadiums) — 1 shown as representative
  drawPort(
      ctx, 0, HEADER_HEIGHT * SCALE + 10 * SCALE, PORT_RADIUS * SCALE, true);
  // Output ports (right edge)
  drawPort(
      ctx, nodeW, HEADER_HEIGHT * SCALE + 10 * SCALE, PORT_RADIUS * SCALE,
      false);
}

function drawPort(ctx, edgeX, centreY, r, isInput) {
  const fillColor = isInput ? COLOR_PORT_IN : COLOR_PORT_OUT;
  const borderColor = isInput ? COLOR_PORT_IN_BD : COLOR_PORT_OUT_BD;

  ctx.save();
  // Clip to the half facing inward
  ctx.beginPath();
  if (isInput)
    ctx.rect(edgeX, centreY - r, r, r * 2);
  else
    ctx.rect(edgeX - r, centreY - r, r, r * 2);
  ctx.clip();

  // Full rounded rect centered such that one half is visible
  const rx = isInput ? edgeX - r : edgeX - r;
  const rw = r * 2;
  const rh = r * 2;
  const corner = r;
  ctx.beginPath();
  ctx.roundRect(rx, centreY - r, rw, rh, corner);
  ctx.fillStyle = fillColor;
  ctx.fill();
  ctx.strokeStyle = borderColor;
  ctx.lineWidth = 1.5;
  ctx.stroke();
  ctx.restore();
}

function drawSubGrid(ctx, body, gw, gh, sx, sy) {
  ctx.strokeStyle = 'rgba(255,255,255,0.04)';
  ctx.lineWidth = 0.5;
  const cols = gw * SUBS_PER_UNIT;
  const rows = gh * SUBS_PER_UNIT;
  for (let c = 0; c <= cols; c++) {
    const x = body.x + c * sx;
    ctx.beginPath();
    ctx.moveTo(x, body.y);
    ctx.lineTo(x, body.y + body.h);
    ctx.stroke();
  }
  for (let r = 0; r <= rows; r++) {
    const y = body.y + r * sy;
    ctx.beginPath();
    ctx.moveTo(body.x, y);
    ctx.lineTo(body.x + body.w, y);
    ctx.stroke();
  }
}

function drawElement(ctx, el, r, selected) {
  switch (el.type) {
    case 'RotarySlider':
      drawRotary(ctx, el, r);
      break;
    case 'Label':
      drawLabel(ctx, el, r);
      break;
    case 'Toggle':
      drawToggle(ctx, el, r);
      break;
    case 'PushButton':
      drawToggle(ctx, el, r);
      break;
    case 'ComboBox':
      drawCombo(ctx, el, r);
      break;
    case 'Custom':
      drawCustom(ctx, el, r);
      break;
    default:
      drawCustom(ctx, el, r);
      break;
  }
}

// ============================================================
// Element Drawers — match JUCE visual style
// ============================================================

function drawRotary(ctx, el, r) {
  // Mirror ArpsLookAndFeel::drawRotarySlider
  // The knob occupies the full bounds. The text appears inside the arc.
  const cx = r.x + r.w / 2;
  const cy = r.y + r.h / 2;
  const radius = (Math.min(r.w, r.h) / 2) - 2;
  const trackWidth = radius * 0.4;

  // Demo at mid-position
  const sliderPos = 0.5;
  const angle = ROTARY_START + sliderPos * (ROTARY_END - ROTARY_START);

  // Background ellipse
  ctx.fillStyle = 'rgba(0,0,0,0.1)';
  ctx.beginPath();
  ctx.ellipse(cx, cy, radius, radius, 0, 0, Math.PI * 2);
  ctx.fill();

  // Background track arc
  ctx.beginPath();
  ctx.arc(cx, cy, radius, ROTARY_START, ROTARY_END);
  ctx.strokeStyle = COLOR_TRACK_DARK;
  ctx.lineWidth = trackWidth;
  ctx.lineCap = 'round';
  ctx.stroke();

  // Neon glow arc (wider, semi-transparent)
  ctx.beginPath();
  ctx.arc(cx, cy, radius, ROTARY_START, angle);
  ctx.strokeStyle = 'rgba(13, 240, 227, 0.2)';
  ctx.lineWidth = trackWidth * 1.5;
  ctx.stroke();

  // Neon fill arc
  ctx.beginPath();
  ctx.arc(cx, cy, radius, ROTARY_START, angle);
  ctx.strokeStyle = COLOR_NEON;
  ctx.lineWidth = trackWidth;
  ctx.stroke();

  // Center value text (radius * 0.9 font, matches JUCE)
  const fontSize = Math.max(6, radius * 0.9);
  ctx.fillStyle = COLOR_TEXT;
  ctx.font = `bold ${fontSize}px "Segoe UI", sans-serif`;
  ctx.textAlign = 'center';
  ctx.textBaseline = 'middle';
  ctx.fillText(el.label || '...', cx, cy + 1);

  // Indicator dot
  const dotX = cx + radius * Math.cos(angle - Math.PI / 2);
  const dotY = cy + radius * Math.sin(angle - Math.PI / 2);
  ctx.fillStyle = COLOR_TEXT;
  ctx.beginPath();
  ctx.arc(dotX, dotY, 3, 0, Math.PI * 2);
  ctx.fill();
}

function drawLabel(ctx, el, r) {
  // Mirror NodeBlock: 13pt bold, centred, with optional colorHex
  let color = COLOR_LABEL;
  let bold = false;
  if (el.colorHex) {
    // Strip leading 'ff' alpha prefix if present (aarrggbb → rrggbb)
    const hex =
        el.colorHex.length === 8 ? el.colorHex.substring(2) : el.colorHex;
    color = '#' + hex;
    bold = true;
  }

  const fontSize = Math.max(6, 13 * SCALE / 3);
  ctx.fillStyle = color;
  ctx.font = `${bold ? 'bold ' : ''}${fontSize}px "Segoe UI", sans-serif`;
  ctx.textAlign = 'center';
  ctx.textBaseline = 'middle';

  // Clip to label bounds
  ctx.save();
  ctx.rect(r.x, r.y, r.w, r.h);
  ctx.clip();
  ctx.fillText(el.label || '', r.x + r.w / 2, r.y + r.h / 2);
  ctx.restore();
}

function drawToggle(ctx, el, r) {
  // Mirrors TextButton (getForegroundSlate background, white text, neon border
  // on hover)
  const R = 4 * SCALE / 3;

  ctx.fillStyle = COLOR_BG_NODE;
  roundRect(ctx, r.x, r.y, r.w, r.h, R);
  ctx.fill();

  ctx.strokeStyle = COLOR_NEON_DIM;
  ctx.lineWidth = 1;
  roundRect(ctx, r.x + 0.5, r.y + 0.5, r.w - 1, r.h - 1, R);
  ctx.stroke();

  const fontSize = Math.max(6, 11 * SCALE / 3);
  ctx.fillStyle = COLOR_TEXT;
  ctx.font = `${fontSize}px "Segoe UI", sans-serif`;
  ctx.textAlign = 'center';
  ctx.textBaseline = 'middle';
  ctx.save();
  ctx.rect(r.x, r.y, r.w, r.h);
  ctx.clip();
  ctx.fillText(el.label || '', r.x + r.w / 2, r.y + r.h / 2);
  ctx.restore();
}

function drawCombo(ctx, el, r) {
  // Mirrors ComboBox (getForegroundSlate, white text, neon arrow)
  const R = 2 * SCALE / 3;

  ctx.fillStyle = COLOR_COMBO_BG;
  roundRect(ctx, r.x, r.y, r.w, r.h, R);
  ctx.fill();

  // Subtle border
  ctx.strokeStyle = COLOR_COMBO_BORDER;
  ctx.lineWidth = 1;
  roundRect(ctx, r.x + 0.5, r.y + 0.5, r.w - 1, r.h - 1, R);
  ctx.stroke();

  // Label text (first option or custom label)
  const displayText =
      (el.options && el.options.length > 0) ? el.options[0] : (el.label || '');
  const fontSize = Math.max(6, 11 * SCALE / 3);
  ctx.fillStyle = COLOR_TEXT;
  ctx.font = `${fontSize}px "Segoe UI", sans-serif`;
  ctx.textAlign = 'left';
  ctx.textBaseline = 'middle';
  ctx.save();
  ctx.rect(r.x, r.y, r.w, r.h);
  ctx.clip();
  ctx.fillText(displayText, r.x + 4 * SCALE / 3, r.y + r.h / 2);
  ctx.restore();

  // Neon dropdown arrow (right side)
  const arrowSize = Math.max(4, r.h * 0.4);
  const ax = r.x + r.w - arrowSize - 3 * SCALE / 3;
  const ay = r.y + r.h / 2;
  ctx.fillStyle = COLOR_NEON;
  ctx.beginPath();
  ctx.moveTo(ax, ay - arrowSize * 0.3);
  ctx.lineTo(ax + arrowSize, ay - arrowSize * 0.3);
  ctx.lineTo(ax + arrowSize / 2, ay + arrowSize * 0.4);
  ctx.closePath();
  ctx.fill();
}

function drawCustom(ctx, el, r) {
  ctx.fillStyle = 'rgba(80, 80, 80, 0.4)';
  ctx.strokeStyle = 'rgba(140, 140, 140, 0.6)';
  ctx.lineWidth = 1;
  ctx.setLineDash([4, 3]);
  ctx.strokeRect(r.x + 0.5, r.y + 0.5, r.w - 1, r.h - 1);
  ctx.setLineDash([]);

  const fontSize = Math.max(6, 11 * SCALE / 3);
  ctx.fillStyle = 'rgba(180,180,180,0.6)';
  ctx.font = `italic ${fontSize}px "Segoe UI", sans-serif`;
  ctx.textAlign = 'center';
  ctx.textBaseline = 'middle';
  ctx.fillText(
      el.customType || el.type || 'Custom', r.x + r.w / 2, r.y + r.h / 2);
}

// ============================================================
// Interaction Overlay (DOM layer for drag/resize handles)
// ============================================================

let isDragging = false;
let isResizing = false;
let dragStartX = 0, dragStartY = 0;
let dragStartBounds = null;
let interactIdx = -1;

function getBodyAndPitch() {
  if (!currentData) return null;
  const gw = currentData.gridWidth || 1;
  const gh = currentData.gridHeight || 1;
  const {w: nw, h: nh} = nodePixelSize(gw, gh);
  const body = bodyRegion(nw, nh);
  const {sx, sy} = subGridPitch(body.w, body.h, gw, gh);
  return {body, sx, sy};
}

function renderOverlay(body, sx, sy) {
  overlayDiv.innerHTML = '';
  // Position overlay to exactly cover the canvas
  overlayDiv.style.width = previewCanvas.width + 'px';
  overlayDiv.style.height = previewCanvas.height + 'px';
  overlayDiv.style.left = previewCanvas.offsetLeft + 'px';
  overlayDiv.style.top = previewCanvas.offsetTop + 'px';

  currentData.elements.forEach((el, index) => {
    const r = elementRect(el, body, sx, sy);
    const handle = document.createElement('div');
    handle.className = 'overlay-element';
    handle.style.left = r.x + 'px';
    handle.style.top = r.y + 'px';
    handle.style.width = r.w + 'px';
    handle.style.height = r.h + 'px';
    // pointer-events:none so all mouse events fall through to the canvas
    handle.style.pointerEvents = 'none';

    if (index === selectedElementIndex) {
      handle.style.outline = `1.5px dashed ${COLOR_NEON}`;
      // Resize grip indicator (visual only)
      const grip = document.createElement('div');
      grip.className = 'resize-grip';
      handle.appendChild(grip);
    }

    overlayDiv.appendChild(handle);
  });
}

// Hit-test: which element was clicked?
function elementAtPoint(px, py) {
  const g = getBodyAndPitch();
  if (!g) return -1;
  const {body, sx, sy} = g;

  // Iterate in reverse (top elements first)
  for (let i = currentData.elements.length - 1; i >= 0; i--) {
    const r = elementRect(currentData.elements[i], body, sx, sy);
    if (px >= r.x && px <= r.x + r.w && py >= r.y && py <= r.y + r.h) return i;
  }
  return -1;
}

function onCanvasMouseDown(e) {
  const rect = previewCanvas.getBoundingClientRect();
  const px = e.clientX - rect.left;
  const py = e.clientY - rect.top;

  const idx = elementAtPoint(px, py);

  if (idx === -1) {
    selectedElementIndex = -1;
    renderAll();
    renderProperties();
    return;
  }

  selectedElementIndex = idx;
  interactIdx = idx;
  dragStartX = e.clientX;
  dragStartY = e.clientY;
  dragStartBounds = [...currentData.elements[idx].gridBounds];

  // Check if we're near the bottom-right (resize zone = 10px corner)
  const g = getBodyAndPitch();
  const r = elementRect(currentData.elements[idx], g.body, g.sx, g.sy);
  const inResizeZone = (px > r.x + r.w - 10) && (py > r.y + r.h - 10);
  isResizing = inResizeZone;
  isDragging = !inResizeZone;

  renderAll();
  renderProperties();
  e.preventDefault();
}

function onCanvasMouseMove(e) {
  if (isDragging || isResizing) return;

  const rect = previewCanvas.getBoundingClientRect();
  const px = e.clientX - rect.left;
  const py = e.clientY - rect.top;

  const idx = elementAtPoint(px, py);
  if (idx !== -1) {
    const g = getBodyAndPitch();
    if (g) {
      const r = elementRect(currentData.elements[idx], g.body, g.sx, g.sy);
      // 10px corner for resize grip hit
      if (px > r.x + r.w - 10 && py > r.y + r.h - 10) {
        previewCanvas.style.cursor = 'se-resize';
        return;
      }
    }
    previewCanvas.style.cursor = 'move';
    return;
  }
  previewCanvas.style.cursor = 'crosshair';
}

function onDocMouseMove(e) {
  if (!isDragging && !isResizing) return;
  if (interactIdx < 0) return;

  const g = getBodyAndPitch();
  if (!g) return;
  const {sx, sy} = g;

  const dx = e.clientX - dragStartX;
  const dy = e.clientY - dragStartY;
  const gridDx = Math.round(dx / sx);
  const gridDy = Math.round(dy / sy);

  const b = currentData.elements[interactIdx].gridBounds;
  if (isDragging) {
    b[0] = Math.max(0, dragStartBounds[0] + gridDx);
    b[1] = Math.max(0, dragStartBounds[1] + gridDy);
  } else if (isResizing) {
    b[2] = Math.max(1, dragStartBounds[2] + gridDx);
    b[3] = Math.max(1, dragStartBounds[3] + gridDy);
  }

  renderAll();
}

function onDocMouseUp() {
  if (isDragging || isResizing) renderProperties();
  isDragging = false;
  isResizing = false;
  interactIdx = -1;
}

// ============================================================
// Properties Panel
// ============================================================

function renderProperties() {
  if (selectedElementIndex === -1 || !currentData) {
    propsContent.innerHTML =
        '<p>Select an element on the canvas to edit its properties.</p>';
    return;
  }

  const el = currentData.elements[selectedElementIndex];
  let html = `
        <div class="prop-row">
            <label>Type</label>
            <input type="text" value="${el.type}" disabled>
        </div>
        ${createInput('label', 'Label', el.label || '')}
        ${createBoundsInputs(el.gridBounds)}
    `;

  if (el.type === 'RotarySlider' || el.type === 'Slider') {
    html += createInput(
        'minValue', 'Min Value', el.minValue !== undefined ? el.minValue : 0,
        'number');
    html += createInput(
        'maxValue', 'Max Value', el.maxValue !== undefined ? el.maxValue : 1,
        'number');
    html += createInput(
        'floatMin', 'Float Min', el.floatMin !== undefined ? el.floatMin : '',
        'number', 'any');
    html += createInput(
        'floatMax', 'Float Max', el.floatMax !== undefined ? el.floatMax : '',
        'number', 'any');
    html += createInput(
        'step', 'Step', el.step !== undefined ? el.step : '', 'number', 'any');
    html += createCheckbox('bipolar', 'Bipolar (Centered Zero)', el.bipolar);
  }

  if (el.type === 'Label') {
    html +=
        createInput('colorHex', 'Text Color Hex (aarrggbb)', el.colorHex || '');
  }

  if (el.type === 'Custom') {
    html += createInput('customType', 'Custom Class ID', el.customType || '');
  }

  html +=
      `<button id="btn-delete-element" style="background-color:#c93434;margin-top:15px;width:100%">Delete Element</button>`;
  propsContent.innerHTML = html;

  propsContent.querySelectorAll('.dyn-prop').forEach(inp => {
    inp.addEventListener('change', (e) => {
      const key = e.target.dataset.key;
      let val = e.target.value;
      if (e.target.type === 'number') val = parseFloat(val);
      if (e.target.type === 'checkbox') val = e.target.checked;
      if (val === '' || (isNaN(val) && e.target.type === 'number'))
        delete el[key];
      else
        el[key] = val;
      renderAll();
    });
  });

  propsContent.querySelectorAll('.dyn-bound').forEach(inp => {
    inp.addEventListener('change', (e) => {
      const idx = parseInt(e.target.dataset.idx);
      el.gridBounds[idx] = parseInt(e.target.value) || 0;
      renderAll();
    });
  });

  document.getElementById('btn-delete-element')
      .addEventListener('click', () => {
        if (confirm('Delete this element?')) {
          currentData.elements.splice(selectedElementIndex, 1);
          selectedElementIndex = -1;
          renderAll();
          renderProperties();
        }
      });
}

function createInput(key, title, val, type = 'text', step = '') {
  return `<div class="prop-row">
        <label>${title}</label>
        <input type="${type}" ${
      step ? `step="${step}"` :
             ''} class="dyn-prop" data-key="${key}" value="${val}">
    </div>`;
}

function createCheckbox(key, title, checked) {
  return `<div class="prop-row">
        <label>${title}</label>
        <input type="checkbox" class="dyn-prop" data-key="${key}" ${
      checked ? 'checked' : ''}>
    </div>`;
}

function createBoundsInputs(bounds) {
  if (!bounds || bounds.length !== 4) return '';
  return `<div class="prop-row">
        <label>X, Y, Width, Height (grid subs)</label>
        <div style="display:flex;gap:5px;">
            <input type="number" class="dyn-bound" data-idx="0" value="${
      bounds[0]}" style="width:44px">
            <input type="number" class="dyn-bound" data-idx="1" value="${
      bounds[1]}" style="width:44px">
            <input type="number" class="dyn-bound" data-idx="2" value="${
      bounds[2]}" style="width:44px">
            <input type="number" class="dyn-bound" data-idx="3" value="${
      bounds[3]}" style="width:44px">
        </div>
    </div>`;
}

// ============================================================
// Utility
// ============================================================

function roundRect(ctx, x, y, w, h, r) {
  ctx.beginPath();
  ctx.moveTo(x + r, y);
  ctx.lineTo(x + w - r, y);
  ctx.arcTo(x + w, y, x + w, y + r, r);
  ctx.lineTo(x + w, y + h - r);
  ctx.arcTo(x + w, y + h, x + w - r, y + h, r);
  ctx.lineTo(x + r, y + h);
  ctx.arcTo(x, y + h, x, y + h - r, r);
  ctx.lineTo(x, y + r);
  ctx.arcTo(x, y, x + r, y, r);
  ctx.closePath();
}
