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
// getForegroundSlate.brighter(0.1) ≈ #141d28
const COLOR_BG_NODE_UNFOLDED = '#141d28';
const COLOR_NEON = '#0df0e3';     // getNeonColor
const COLOR_NEON_DIM = 'rgba(13, 240, 227, 0.3)';
const COLOR_TRACK_DARK = '#333333';  // rotarySliderOutlineColourId
// Port colours default to Agnostic (grey) — type not stored in JSON
const COLOR_PORT_IN = '#888888';
const COLOR_PORT_IN_BD = '#555555';
const COLOR_PORT_OUT = '#888888';
const COLOR_PORT_OUT_BD = '#555555';
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
let previewMode = 0; // 0: folded, 1: unfolded
let selectedElementIndex = -1;

// ============================================================
// Undo / Redo
// ============================================================
const undoStack = [];
const redoStack = [];

function pushUndo() {
  undoStack.push(JSON.stringify(currentData));
  if (undoStack.length > 50) undoStack.shift();
  redoStack.length = 0;
  updateUndoButtons();
}

function undo() {
  if (!undoStack.length) return;
  redoStack.push(JSON.stringify(currentData));
  currentData = JSON.parse(undoStack.pop());
  selectedElementIndex = -1;
  renderAll();
  renderProperties();
  updateUndoButtons();
}

function redo() {
  if (!redoStack.length) return;
  undoStack.push(JSON.stringify(currentData));
  currentData = JSON.parse(redoStack.pop());
  selectedElementIndex = -1;
  renderAll();
  renderProperties();
  updateUndoButtons();
}

function updateUndoButtons() {
  const btnUndo = document.getElementById('btn-undo');
  const btnRedo = document.getElementById('btn-redo');
  if (btnUndo) btnUndo.disabled = undoStack.length === 0;
  if (btnRedo) btnRedo.disabled = redoStack.length === 0;
}

const btnOpen = document.getElementById('btn-open-dir');
const selNode = document.getElementById('sel-node');
const btnSave = document.getElementById('btn-save');
document.getElementById('btn-undo').addEventListener('click', undo);
document.getElementById('btn-redo').addEventListener('click', redo);
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
    undoStack.length = 0;
    redoStack.length = 0;
    updateUndoButtons();

    document.getElementById('node-w').value = currentData.gridWidth || 1;
    document.getElementById('node-h').value = currentData.gridHeight || 1;
    document.getElementById('sel-preview-mode').value = "0";
    previewMode = 0;

    // Show/hide unfold info
    const hasUnfold = (currentData.unfoldedElements || []).length > 0;
    const unfoldInfo = document.getElementById('unfold-info');
    if (unfoldInfo) {
      unfoldInfo.textContent = hasUnfold
        ? `Unfolded size: ${(currentData.gridWidth || 1) + 2} × ${(currentData.gridHeight || 1) + 2} (auto)`
        : 'No unfoldedElements — add some to enable unfold mode.';
    }

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

document.getElementById('sel-preview-mode').addEventListener('change', (e) => {
  previewMode = parseInt(e.target.value);
  selectedElementIndex = -1;
  renderAll();
  renderProperties();
});

btnSave.addEventListener('click', async () => {
  if (!currentData || !currentNodeKey) return;
  try {
    statusMsg.textContent = 'Saving...';
    currentData.elements.forEach(el => {
      el.gridBounds = el.gridBounds.map(v => Math.round(v));
    });
    if (currentData.unfoldedElements) {
      currentData.unfoldedElements.forEach(el => {
        el.gridBounds = el.gridBounds.map(v => Math.round(v));
      });
    }
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
    pushUndo();
    currentData.gridWidth = parseInt(e.target.value);
    // Update unfold info
    const hasUnfold = (currentData.unfoldedElements || []).length > 0;
    const unfoldInfo = document.getElementById('unfold-info');
    if (unfoldInfo && hasUnfold) {
      unfoldInfo.textContent = `Unfolded size: ${currentData.gridWidth + 2} × ${(currentData.gridHeight || 1) + 2} (auto)`;
    }
    renderAll();
  }
});
document.getElementById('node-h').addEventListener('change', (e) => {
  if (currentData) {
    pushUndo();
    currentData.gridHeight = parseInt(e.target.value);
    const hasUnfold = (currentData.unfoldedElements || []).length > 0;
    const unfoldInfo = document.getElementById('unfold-info');
    if (unfoldInfo && hasUnfold) {
      unfoldInfo.textContent = `Unfolded size: ${(currentData.gridWidth || 1) + 2} × ${currentData.gridHeight + 2} (auto)`;
    }
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

// Body region for the compact panel (or the full panel header area).
// Mirrors NodeBlock::resized() compact layout:
//   startX = PORT_MARGIN, startY = HEADER_HEIGHT
//   bodyWidth = width - PORT_MARGIN*2, bodyHeight = height - HEADER_HEIGHT - BOTTOM_PAD
function bodyRegion(nodeW, nodeH) {
  return {
    x: PORT_MARGIN * SCALE,
    y: HEADER_HEIGHT * SCALE,
    w: nodeW - PORT_MARGIN * 2 * SCALE,
    h: nodeH - HEADER_HEIGHT * SCALE - BOTTOM_PAD * SCALE,
  };
}

// Body region for unfolded elements — spans the full expanded panel.
// Mirrors the unfolded layout in NodeBlock::resized():
//   startX = PORT_MARGIN, startY = HEADER_HEIGHT (header at top of full panel)
//   bodyWidth = getWidth() - PORT_MARGIN*2, bodyHeight = getHeight() - HEADER_HEIGHT - 4
function unfoldedBodyRegion(nodeW, nodeH) {
  return {
    x: PORT_MARGIN * SCALE,
    y: HEADER_HEIGHT * SCALE,
    w: nodeW - PORT_MARGIN * 2 * SCALE,
    h: nodeH - HEADER_HEIGHT * SCALE - BOTTOM_PAD * SCALE,
  };
}

// Body region for compact elements shown as ghosts inside the unfolded panel.
// Mirrors NodeBlock::resized() compact layout but with the compact body inset:
//   compact body offset: (GridPitch, GridPitch) within the expanded panel
//   after header: startX = GridPitch + PORT_MARGIN, startY = GridPitch + HEADER_HEIGHT
function compactGhostBodyRegion(baseGw, baseGh) {
  const cpx = GRID_PITCH * SCALE;
  const cpy = GRID_PITCH * SCALE;
  const cpw = (baseGw * GRID_PITCH - TRAMLINE_MARGIN) * SCALE;
  const cph = (baseGh * GRID_PITCH - TRAMLINE_MARGIN) * SCALE;
  return {
    x: cpx + PORT_MARGIN * SCALE,
    y: cpy + HEADER_HEIGHT * SCALE,
    w: cpw - PORT_MARGIN * 2 * SCALE,
    h: cph - HEADER_HEIGHT * SCALE - BOTTOM_PAD * SCALE,
  };
}

function subGridPitch(bodyW, bodyH, gridW, gridH) {
  const cols = gridW * SUBS_PER_UNIT;
  const rows = gridH * SUBS_PER_UNIT;
  return { sx: bodyW / cols, sy: bodyH / rows };
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

function isUnfoldedMode() {
  return previewMode === 1 && currentData &&
    (currentData.unfoldedElements || []).length > 0;
}

function renderAll() {
  if (!currentData || !previewCtx) return;

  const baseGw = currentData.gridWidth || 1;
  const baseGh = currentData.gridHeight || 1;
  const baseElements = currentData.elements || [];
  const unfoldedElements = currentData.unfoldedElements || [];
  const unfolded = isUnfoldedMode();

  // Unfolded dimensions are always base + 2 on each axis
  const gw = unfolded ? baseGw + 2 : baseGw;
  const gh = unfolded ? baseGh + 2 : baseGh;

  const { w: nodeW, h: nodeH } = nodePixelSize(gw, gh);
  previewCanvas.width = nodeW;
  previewCanvas.height = nodeH;

  const ctx = previewCtx;

  // --- Node body ---
  drawNodeBody(ctx, nodeW, nodeH, unfolded, baseGw, baseGh);

  if (unfolded) {
    // Subgrid spans the full unfolded panel
    const fullBody = unfoldedBodyRegion(nodeW, nodeH);
    const { sx, sy } = subGridPitch(fullBody.w, fullBody.h, gw, gh);
    drawSubGrid(ctx, fullBody, gw, gh, sx, sy);

    // Compact elements shown dimmed in their compact body sub-area
    const ghostBody = compactGhostBodyRegion(baseGw, baseGh);
    const { sx: gsx, sy: gsy } = subGridPitch(ghostBody.w, ghostBody.h, baseGw, baseGh);
    ctx.globalAlpha = 0.35;
    baseElements.forEach((el) => {
      const r = elementRect(el, ghostBody, gsx, gsy);
      drawElement(ctx, el, r, false);
    });
    ctx.globalAlpha = 1.0;

    // Unfolded elements at full opacity
    unfoldedElements.forEach((el, i) => {
      const r = elementRect(el, fullBody, sx, sy);
      const selected = (i === selectedElementIndex);
      drawElement(ctx, el, r, selected);
    });

    // Selected element border
    if (selectedElementIndex >= 0 && selectedElementIndex < unfoldedElements.length) {
      const el = unfoldedElements[selectedElementIndex];
      const r = elementRect(el, fullBody, sx, sy);
      ctx.strokeStyle = COLOR_NEON;
      ctx.lineWidth = 1.5;
      ctx.setLineDash([4, 3]);
      ctx.strokeRect(r.x + 0.5, r.y + 0.5, r.w - 1, r.h - 1);
      ctx.setLineDash([]);
    }

    renderOverlay(fullBody, sx, sy, unfoldedElements);
  } else {
    const body = bodyRegion(nodeW, nodeH);
    const { sx, sy } = subGridPitch(body.w, body.h, baseGw, baseGh);
    drawSubGrid(ctx, body, baseGw, baseGh, sx, sy);

    baseElements.forEach((el, i) => {
      const r = elementRect(el, body, sx, sy);
      const selected = (i === selectedElementIndex);
      drawElement(ctx, el, r, selected);
    });

    if (selectedElementIndex >= 0 && selectedElementIndex < baseElements.length) {
      const el = baseElements[selectedElementIndex];
      const r = elementRect(el, body, sx, sy);
      ctx.strokeStyle = COLOR_NEON;
      ctx.lineWidth = 1.5;
      ctx.setLineDash([4, 3]);
      ctx.strokeRect(r.x + 0.5, r.y + 0.5, r.w - 1, r.h - 1);
      ctx.setLineDash([]);
    }

    renderOverlay(body, sx, sy, baseElements);
  }

  // --- Edit mode badge ---
  const badge = document.getElementById('edit-mode-badge');
  if (badge) {
    if (unfolded) {
      badge.textContent = 'Editing: Unfolded Panel';
      badge.className = 'mode-badge mode-badge--extended';
    } else {
      badge.textContent = 'Editing: Base Panel';
      badge.className = 'mode-badge';
    }
  }
}

// Draws a dashed rounded rect at the compact body boundary within the unfolded panel.
function drawCompactBodyHint(ctx, baseGw, baseGh) {
  const cpx = GRID_PITCH * SCALE;
  const cpy = GRID_PITCH * SCALE;
  const cpw = (baseGw * GRID_PITCH - TRAMLINE_MARGIN) * SCALE;
  const cph = (baseGh * GRID_PITCH - TRAMLINE_MARGIN) * SCALE;
  const R = 4 * (SCALE / 3);

  ctx.save();
  ctx.strokeStyle = 'rgba(136, 119, 85, 0.5)';
  ctx.lineWidth = 1.2;
  ctx.setLineDash([5, 3]);
  roundRect(ctx, cpx + 1, cpy + 1, cpw - 2, cph - 2, R);
  ctx.stroke();
  ctx.setLineDash([]);
  ctx.restore();
}

function drawNodeBody(ctx, nodeW, nodeH, unfolded, baseGw, baseGh) {
  const R = 6 * (SCALE / 3);
  const Ru = 8 * (SCALE / 3);

  if (unfolded) {
    // Brighter slate for the expanded panel
    ctx.fillStyle = COLOR_BG_NODE_UNFOLDED;
    roundRect(ctx, 0, 0, nodeW, nodeH, Ru);
    ctx.fill();

    // Dashed compact body hint
    drawCompactBodyHint(ctx, baseGw, baseGh);

    // Border around full panel
    ctx.strokeStyle = COLOR_NEON_DIM;
    ctx.lineWidth = 1.5;
    roundRect(ctx, 0.5, 0.5, nodeW - 1, nodeH - 1, Ru);
    ctx.stroke();

    // Header divider at top of full panel
    ctx.strokeStyle = 'rgba(13, 240, 227, 0.2)';
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.moveTo(0, HEADER_HEIGHT * SCALE);
    ctx.lineTo(nodeW, HEADER_HEIGHT * SCALE);
    ctx.stroke();

    // Ports at outer edges of full panel
    drawPort(ctx, 0, HEADER_HEIGHT * SCALE + 10 * SCALE, PORT_RADIUS * SCALE, true);
    drawPort(ctx, nodeW, HEADER_HEIGHT * SCALE + 10 * SCALE, PORT_RADIUS * SCALE, false);
  } else {
    ctx.fillStyle = COLOR_BG_NODE;
    roundRect(ctx, 0, 0, nodeW, nodeH, R);
    ctx.fill();

    ctx.strokeStyle = COLOR_NEON_DIM;
    ctx.lineWidth = 1.5;
    roundRect(ctx, 0.5, 0.5, nodeW - 1, nodeH - 1, R);
    ctx.stroke();

    ctx.strokeStyle = 'rgba(13, 240, 227, 0.2)';
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.moveTo(0, HEADER_HEIGHT * SCALE);
    ctx.lineTo(nodeW, HEADER_HEIGHT * SCALE);
    ctx.stroke();

    drawPort(ctx, 0, HEADER_HEIGHT * SCALE + 10 * SCALE, PORT_RADIUS * SCALE, true);
    drawPort(ctx, nodeW, HEADER_HEIGHT * SCALE + 10 * SCALE, PORT_RADIUS * SCALE, false);
  }

  // Title text
  const nodeName = currentNodeKey
    ? currentNodeKey.split('/').pop().replace('.json', '').replace('Node', ' Node')
    : 'Node';
  ctx.fillStyle = COLOR_TEXT;
  ctx.font = `bold ${Math.max(9, 14 * SCALE / 3)}px "Segoe UI", sans-serif`;
  ctx.textAlign = 'left';
  ctx.textBaseline = 'middle';
  ctx.fillText(nodeName, 8 * SCALE / 3, HEADER_HEIGHT * SCALE / 2);

  // Delete button placeholder (top-right)
  const btnSize = (HEADER_HEIGHT - 6) * SCALE;
  const btnX = nodeW - btnSize - 3 * (SCALE / 3);
  const btnY = 3 * (SCALE / 3);
  ctx.fillStyle = 'rgba(0,0,0,0.2)';
  roundRect(ctx, btnX, btnY, btnSize, btnSize, 3);
  ctx.fill();
  ctx.fillStyle = 'rgba(255,255,255,0.5)';
  ctx.font = `${Math.max(7, 10 * SCALE / 3)}px sans-serif`;
  ctx.textAlign = 'center';
  ctx.textBaseline = 'middle';
  ctx.fillText('×', btnX + btnSize / 2, btnY + btnSize / 2);

  // Expand button — shown for nodes that have unfoldedElements
  const hasUnfold = currentData && (currentData.unfoldedElements || []).length > 0;
  if (hasUnfold) {
    const gap = 4 * (SCALE / 3);
    const expBtnX = btnX - btnSize - gap;
    ctx.fillStyle = 'rgba(13, 240, 227, 0.12)';
    roundRect(ctx, expBtnX, btnY, btnSize, btnSize, 3);
    ctx.fill();
    ctx.strokeStyle = 'rgba(13, 240, 227, 0.5)';
    ctx.lineWidth = 1;
    roundRect(ctx, expBtnX + 0.5, btnY + 0.5, btnSize - 1, btnSize - 1, 3);
    ctx.stroke();
    ctx.fillStyle = unfolded ? COLOR_NEON : 'rgba(13, 240, 227, 0.7)';
    ctx.font = `${Math.max(7, 10 * SCALE / 3)}px sans-serif`;
    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';
    ctx.fillText(unfolded ? '<' : '>', expBtnX + btnSize / 2, btnY + btnSize / 2);
  }
}

function drawPort(ctx, edgeX, centreY, r, isInput) {
  const fillColor = isInput ? COLOR_PORT_IN : COLOR_PORT_OUT;
  const borderColor = isInput ? COLOR_PORT_IN_BD : COLOR_PORT_OUT_BD;

  ctx.save();
  ctx.beginPath();
  if (isInput)
    ctx.rect(edgeX, centreY - r, r, r * 2);
  else
    ctx.rect(edgeX - r, centreY - r, r, r * 2);
  ctx.clip();

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
  const cx = r.x + r.w / 2;
  const cy = r.y + r.h / 2;
  const radius = (Math.min(r.w, r.h) / 2) - 2;
  const trackWidth = radius * 0.4;

  const sliderPos = 0.5;
  const angle = ROTARY_START + sliderPos * (ROTARY_END - ROTARY_START);

  ctx.fillStyle = 'rgba(0,0,0,0.1)';
  ctx.beginPath();
  ctx.ellipse(cx, cy, radius, radius, 0, 0, Math.PI * 2);
  ctx.fill();

  ctx.beginPath();
  ctx.arc(cx, cy, radius, ROTARY_START, ROTARY_END);
  ctx.strokeStyle = COLOR_TRACK_DARK;
  ctx.lineWidth = trackWidth;
  ctx.lineCap = 'round';
  ctx.stroke();

  ctx.beginPath();
  ctx.arc(cx, cy, radius, ROTARY_START, angle);
  ctx.strokeStyle = 'rgba(13, 240, 227, 0.2)';
  ctx.lineWidth = trackWidth * 1.5;
  ctx.stroke();

  ctx.beginPath();
  ctx.arc(cx, cy, radius, ROTARY_START, angle);
  ctx.strokeStyle = COLOR_NEON;
  ctx.lineWidth = trackWidth;
  ctx.stroke();

  const fontSize = Math.max(6, radius * 0.55);
  const midVal = el.floatMin !== undefined
    ? ((el.floatMin + el.floatMax) / 2).toFixed(2)
    : String(Math.round(((el.minValue ?? 0) + (el.maxValue ?? 1)) / 2));
  ctx.fillStyle = COLOR_TEXT;
  ctx.font = `bold ${fontSize}px "Segoe UI", sans-serif`;
  ctx.textAlign = 'center';
  ctx.textBaseline = 'middle';
  ctx.fillText(midVal, cx, cy + 1);

  const dotX = cx + radius * Math.cos(angle - Math.PI / 2);
  const dotY = cy + radius * Math.sin(angle - Math.PI / 2);
  ctx.fillStyle = COLOR_TEXT;
  ctx.beginPath();
  ctx.arc(dotX, dotY, 3, 0, Math.PI * 2);
  ctx.fill();

  if (el.bipolar) {
    const centerAngle = (ROTARY_START + ROTARY_END) / 2 - Math.PI / 2;
    const tickInner = radius - trackWidth * 0.8;
    const tickOuter = radius + trackWidth * 0.5;
    ctx.strokeStyle = 'rgba(255,255,255,0.5)';
    ctx.lineWidth = Math.max(1, trackWidth * 0.3);
    ctx.lineCap = 'round';
    ctx.beginPath();
    ctx.moveTo(cx + tickInner * Math.cos(centerAngle), cy + tickInner * Math.sin(centerAngle));
    ctx.lineTo(cx + tickOuter * Math.cos(centerAngle), cy + tickOuter * Math.sin(centerAngle));
    ctx.stroke();
  }
}

function drawLabel(ctx, el, r) {
  let color = COLOR_LABEL;
  let bold = false;
  if (el.colorHex) {
    const hex = el.colorHex.length === 8 ? el.colorHex.substring(2) : el.colorHex;
    color = '#' + hex;
    bold = true;
  }

  const fontSize = Math.max(6, 13 * SCALE / 3);
  ctx.fillStyle = color;
  ctx.font = `${bold ? 'bold ' : ''}${fontSize}px "Segoe UI", sans-serif`;
  ctx.textAlign = 'center';
  ctx.textBaseline = 'middle';

  ctx.save();
  ctx.rect(r.x, r.y, r.w, r.h);
  ctx.clip();
  ctx.fillText(el.label || '', r.x + r.w / 2, r.y + r.h / 2);
  ctx.restore();
}

function drawToggle(ctx, el, r) {
  const R = 4 * SCALE / 3;

  ctx.fillStyle = COLOR_BG_NODE;
  roundRect(ctx, r.x, r.y, r.w, r.h, R);
  ctx.fill();

  ctx.strokeStyle = COLOR_NEON_DIM;
  ctx.lineWidth = 1;
  roundRect(ctx, r.x + 0.5, r.y + 0.5, r.w - 1, r.h - 1, R);
  ctx.stroke();

  const fontSize = Math.max(6, 11 * SCALE);
  ctx.fillStyle = COLOR_TEXT;
  ctx.font = `bold ${fontSize}px "Segoe UI", sans-serif`;
  ctx.textAlign = 'center';
  ctx.textBaseline = 'middle';
  ctx.save();
  ctx.rect(r.x, r.y, r.w, r.h);
  ctx.clip();
  ctx.fillText(el.label || '', r.x + r.w / 2, r.y + r.h / 2);
  ctx.restore();
}

function drawCombo(ctx, el, r) {
  const R = 2 * SCALE / 3;

  ctx.fillStyle = COLOR_COMBO_BG;
  roundRect(ctx, r.x, r.y, r.w, r.h, R);
  ctx.fill();

  ctx.strokeStyle = COLOR_COMBO_BORDER;
  ctx.lineWidth = 1;
  roundRect(ctx, r.x + 0.5, r.y + 0.5, r.w - 1, r.h - 1, R);
  ctx.stroke();

  const displayText = (el.options && el.options.length > 0) ? el.options[0] : (el.label || '');
  const fontSize = Math.max(6, 11 * SCALE);
  ctx.fillStyle = COLOR_TEXT;
  ctx.font = `${fontSize}px "Segoe UI", sans-serif`;
  ctx.textAlign = 'left';
  ctx.textBaseline = 'middle';
  ctx.save();
  ctx.rect(r.x, r.y, r.w, r.h);
  ctx.clip();
  ctx.fillText(displayText, r.x + 4 * SCALE / 3, r.y + r.h / 2);
  ctx.restore();

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

  const fontSize = Math.max(6, 11 * SCALE);
  ctx.fillStyle = 'rgba(180,180,180,0.6)';
  ctx.font = `italic ${fontSize}px "Segoe UI", sans-serif`;
  ctx.textAlign = 'center';
  ctx.textBaseline = 'middle';
  ctx.fillText(el.customType || el.type || 'Custom', r.x + r.w / 2, r.y + r.h / 2);
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
  const baseGw = currentData.gridWidth || 1;
  const baseGh = currentData.gridHeight || 1;
  const unfolded = isUnfoldedMode();
  const gw = unfolded ? baseGw + 2 : baseGw;
  const gh = unfolded ? baseGh + 2 : baseGh;
  const { w: nw, h: nh } = nodePixelSize(gw, gh);
  const body = unfolded ? unfoldedBodyRegion(nw, nh) : bodyRegion(nw, nh);
  const { sx, sy } = subGridPitch(body.w, body.h, gw, gh);
  return { body, sx, sy };
}

function renderOverlay(body, sx, sy, editElements) {
  overlayDiv.innerHTML = '';
  overlayDiv.style.width = previewCanvas.width + 'px';
  overlayDiv.style.height = previewCanvas.height + 'px';
  overlayDiv.style.left = previewCanvas.offsetLeft + 'px';
  overlayDiv.style.top = previewCanvas.offsetTop + 'px';

  editElements.forEach((el, index) => {
    const r = elementRect(el, body, sx, sy);
    const handle = document.createElement('div');
    handle.className = 'overlay-element';
    handle.style.left = r.x + 'px';
    handle.style.top = r.y + 'px';
    handle.style.width = r.w + 'px';
    handle.style.height = r.h + 'px';
    handle.style.pointerEvents = 'none';

    if (index === selectedElementIndex) {
      handle.style.outline = `1.5px dashed ${COLOR_NEON}`;
      const grip = document.createElement('div');
      grip.className = 'resize-grip';
      handle.appendChild(grip);
    }

    overlayDiv.appendChild(handle);
  });
}

function currentElements() {
  if (!currentData) return [];
  return isUnfoldedMode()
    ? (currentData.unfoldedElements || [])
    : (currentData.elements || []);
}

function elementAtPoint(px, py) {
  const g = getBodyAndPitch();
  if (!g) return -1;
  const { body, sx, sy } = g;
  const elements = currentElements();
  for (let i = elements.length - 1; i >= 0; i--) {
    const r = elementRect(elements[i], body, sx, sy);
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

  const elements = currentElements();
  selectedElementIndex = idx;
  interactIdx = idx;
  dragStartX = e.clientX;
  dragStartY = e.clientY;
  dragStartBounds = [...elements[idx].gridBounds];
  pushUndo();

  const g = getBodyAndPitch();
  const r = elementRect(elements[idx], g.body, g.sx, g.sy);
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

  const g = getBodyAndPitch();
  if (!g) return;

  const idx = elementAtPoint(px, py);
  if (idx !== -1) {
    const elements = currentElements();
    const r = elementRect(elements[idx], g.body, g.sx, g.sy);
    if (px > r.x + r.w - 10 && py > r.y + r.h - 10) {
      previewCanvas.style.cursor = 'se-resize';
      return;
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
  const { sx, sy } = g;

  const dx = e.clientX - dragStartX;
  const dy = e.clientY - dragStartY;
  const gridDx = Math.round(dx / sx);
  const gridDy = Math.round(dy / sy);

  const elements = currentElements();
  const b = elements[interactIdx].gridBounds;
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

const ELEMENT_DEFAULTS = {
  RotarySlider: { type: 'RotarySlider', label: 'param', minValue: 0, maxValue: 1, gridBounds: [0, 0, 13, 13] },
  Label:        { type: 'Label',        label: 'Label',                               gridBounds: [0, 0, 20, 4]  },
  Toggle:       { type: 'Toggle',       label: 'toggle',                              gridBounds: [0, 0, 15, 7]  },
  PushButton:   { type: 'PushButton',   label: 'button',                              gridBounds: [0, 0, 15, 7]  },
  ComboBox:     { type: 'ComboBox',     label: 'combo',  options: ['Option 1'],       gridBounds: [0, 0, 30, 7]  },
  Custom:       { type: 'Custom',       label: 'custom', customType: 'MyComponent',   gridBounds: [0, 0, 20, 20] },
};

function renderProperties() {
  if (selectedElementIndex === -1 || !currentData) {
    const unfolded = isUnfoldedMode();
    const hint = unfolded
      ? '<p>Select an unfolded element to edit.<br><span style="color:#666;font-style:italic">Base elements shown dimmed for reference — switch to Base mode to edit them.</span></p>'
      : '<p>Select an element on the canvas to edit its properties.</p>';

    propsContent.innerHTML = hint + (currentData ? `
      <div style="margin-top:12px">
        <div class="prop-row">
          <label>Add New Element</label>
          <div style="display:flex;gap:6px;align-items:center">
            <select id="new-el-type" style="flex:1;min-width:0">
              ${Object.keys(ELEMENT_DEFAULTS).map(t => `<option value="${t}">${t}</option>`).join('')}
            </select>
            <button id="btn-add-element" style="white-space:nowrap">+ Add</button>
          </div>
        </div>
      </div>` : '');

    if (currentData) {
      document.getElementById('btn-add-element').addEventListener('click', () => {
        const type = document.getElementById('new-el-type').value;
        const targetArray = isUnfoldedMode()
          ? (currentData.unfoldedElements = currentData.unfoldedElements || [])
          : currentData.elements;
        pushUndo();
        const newEl = JSON.parse(JSON.stringify(ELEMENT_DEFAULTS[type]));
        targetArray.push(newEl);
        selectedElementIndex = targetArray.length - 1;
        renderAll();
        renderProperties();
      });
    }
    return;
  }

  const elements = currentElements();
  const el = elements[selectedElementIndex];
  let html = `
        <div class="prop-row">
            <label>Type</label>
            <input type="text" value="${el.type}" disabled>
        </div>
        ${createInput('label', 'Label', el.label || '')}
        ${createBoundsInputs(el.gridBounds)}
    `;

  if (el.type === 'RotarySlider' || el.type === 'Slider') {
    html += createInput('minValue', 'Min Value', el.minValue !== undefined ? el.minValue : 0, 'number');
    html += createInput('maxValue', 'Max Value', el.maxValue !== undefined ? el.maxValue : 1, 'number');
    html += createInput('floatMin', 'Float Min', el.floatMin !== undefined ? el.floatMin : '', 'number', 'any');
    html += createInput('floatMax', 'Float Max', el.floatMax !== undefined ? el.floatMax : '', 'number', 'any');
    html += createInput('step', 'Step', el.step !== undefined ? el.step : '', 'number', 'any');
    html += createCheckbox('bipolar', 'Bipolar (Centered Zero)', el.bipolar);
  }

  if (el.type === 'Label') {
    html += createInput('colorHex', 'Text Color Hex (aarrggbb)', el.colorHex || '');
  }

  if (el.type === 'ComboBox') {
    html += `<div class="prop-row">
      <label>Options (one per line)</label>
      <textarea id="combo-options" rows="5" style="background:#131820;border:1px solid #2e3440;color:white;padding:5px 7px;border-radius:3px;font-size:12px;resize:vertical;width:100%">${(el.options || []).join('\n')}</textarea>
    </div>`;
  }

  if (el.type === 'Custom') {
    html += createInput('customType', 'Custom Class ID', el.customType || '');
  }

  html += `<button id="btn-delete-element" style="background-color:#c93434;margin-top:15px;width:100%">Delete Element</button>`;
  propsContent.innerHTML = html;

  if (el.type === 'ComboBox') {
    document.getElementById('combo-options').addEventListener('change', (e) => {
      pushUndo();
      el.options = e.target.value.split('\n').map(s => s.trim()).filter(s => s.length > 0);
      renderAll();
    });
  }

  propsContent.querySelectorAll('.dyn-prop').forEach(inp => {
    inp.addEventListener('change', (e) => {
      pushUndo();
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
      pushUndo();
      const idx = parseInt(e.target.dataset.idx);
      el.gridBounds[idx] = parseInt(e.target.value) || 0;
      renderAll();
    });
  });

  document.getElementById('btn-delete-element').addEventListener('click', () => {
    if (confirm('Delete this element?')) {
      pushUndo();
      elements.splice(selectedElementIndex, 1);
      selectedElementIndex = -1;
      renderAll();
      renderProperties();
    }
  });
}

function createInput(key, title, val, type = 'text', step = '') {
  return `<div class="prop-row">
        <label>${title}</label>
        <input type="${type}" ${step ? `step="${step}"` : ''} class="dyn-prop" data-key="${key}" value="${val}">
    </div>`;
}

function createCheckbox(key, title, checked) {
  return `<div class="prop-row">
        <label>${title}</label>
        <input type="checkbox" class="dyn-prop" data-key="${key}" ${checked ? 'checked' : ''}>
    </div>`;
}

function createBoundsInputs(bounds) {
  if (!bounds || bounds.length !== 4) return '';
  return `<div class="prop-row">
        <label>X, Y, Width, Height (grid subs)</label>
        <div style="display:flex;gap:5px;">
            <input type="number" class="dyn-bound" data-idx="0" value="${bounds[0]}" style="width:44px">
            <input type="number" class="dyn-bound" data-idx="1" value="${bounds[1]}" style="width:44px">
            <input type="number" class="dyn-bound" data-idx="2" value="${bounds[2]}" style="width:44px">
            <input type="number" class="dyn-bound" data-idx="3" value="${bounds[3]}" style="width:44px">
        </div>
    </div>`;
}

// ============================================================
// Keyboard Shortcuts
// ============================================================
document.addEventListener('keydown', (e) => {
  if (['INPUT', 'SELECT', 'TEXTAREA'].includes(document.activeElement.tagName)) return;

  if (e.ctrlKey && !e.shiftKey && e.key === 'z') {
    undo();
    e.preventDefault();
    return;
  }
  if (e.ctrlKey && (e.key === 'y' || (e.shiftKey && e.key === 'Z'))) {
    redo();
    e.preventDefault();
    return;
  }
  if (e.key === 'Delete' || e.key === 'Backspace') {
    if (selectedElementIndex === -1 || !currentData) return;
    const elements = currentElements();
    if (confirm('Delete this element?')) {
      pushUndo();
      elements.splice(selectedElementIndex, 1);
      selectedElementIndex = -1;
      renderAll();
      renderProperties();
    }
    e.preventDefault();
    return;
  }
  if (e.key === 'Escape') {
    selectedElementIndex = -1;
    renderAll();
    renderProperties();
    return;
  }
});

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
