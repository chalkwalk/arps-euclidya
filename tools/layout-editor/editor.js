// ============================================================
// JUCE Layout Constants (mirrors LayoutConstants.h + NodeBlock.h)
// ============================================================
const GRID_PITCH = 100;      // Layout::GridPitch
const TRAMLINE_MARGIN = 10;  // Layout::TramlineMargin
const HEADER_HEIGHT = 28;    // NodeBlock::HEADER_HEIGHT
const PORT_MARGIN = 14;      // NodeBlock::PORT_MARGIN
const PORT_RADIUS = 8;       // NodeBlock::PORT_RADIUS
const PORT_SPACING = 24;     // NodeBlock::PORT_SPACING
const TAB_HEIGHT = 20;       // NodeBlock::TAB_HEIGHT
const BOTTOM_PAD = 4;
const SUBS_PER_UNIT = 20;  // NodeBlock::resized SUBS_PER_UNIT

// Display scale factor (so the preview isn't tiny; 3 = 300%)
let SCALE = 3;

// ============================================================
// Colors (from ArpsLookAndFeel + NodeBlock paint)
// ============================================================
const COLOR_BG_DARK = '#0b1016';
const COLOR_BG_NODE = '#121a24';
const COLOR_BG_NODE_UNFOLDED = '#141d28';
const COLOR_NEON = '#0df0e3';
const COLOR_NEON_DIM = 'rgba(13, 240, 227, 0.3)';
const COLOR_TRACK_DARK = '#333333';
const COLOR_PORT_IN = '#888888';
const COLOR_PORT_IN_BD = '#555555';
const COLOR_PORT_OUT = '#888888';
const COLOR_PORT_OUT_BD = '#555555';
const COLOR_TEXT = '#ffffff';
const COLOR_LABEL = '#cccccc';
const COLOR_COMBO_BG = '#121a24';
const COLOR_COMBO_BORDER = 'rgba(13, 240, 227, 0.3)';

const ROTARY_START = Math.PI * 1.2;
const ROTARY_END   = Math.PI * 2.8;

// ============================================================
// App State
// ============================================================
let dirHandle = null;
let nodeFiles = new Map();
let currentNodeKey = null;
let currentData = null;

// previewMode: 0 = compact, 1 = unfolded
let previewMode = 0;
let currentCompactTab  = 0;
let currentUnfoldedTab = 0;

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
  renderTabSelector();
  updateUndoButtons();
}

function redo() {
  if (!redoStack.length) return;
  undoStack.push(JSON.stringify(currentData));
  currentData = JSON.parse(redoStack.pop());
  selectedElementIndex = -1;
  renderAll();
  renderProperties();
  renderTabSelector();
  updateUndoButtons();
}

function updateUndoButtons() {
  const btnUndo = document.getElementById('btn-undo');
  const btnRedo = document.getElementById('btn-redo');
  if (btnUndo) btnUndo.disabled = undoStack.length === 0;
  if (btnRedo) btnRedo.disabled = redoStack.length === 0;
}

// ============================================================
// DOM Refs
// ============================================================
const btnOpen    = document.getElementById('btn-open-dir');
const selNode    = document.getElementById('sel-node');
const btnSave    = document.getElementById('btn-save');
document.getElementById('btn-undo').addEventListener('click', undo);
document.getElementById('btn-redo').addEventListener('click', redo);
const statusMsg  = document.getElementById('status-message');
const wrapper    = document.getElementById('canvas-wrapper');
const propsContent = document.getElementById('props-content');
const scaleSlider  = document.getElementById('scale-slider');
const scaleDisplay = document.getElementById('scale-display');

let previewCanvas = null;
let previewCtx    = null;
let overlayDiv    = null;

function initCanvas() {
  wrapper.innerHTML = '';

  previewCanvas = document.createElement('canvas');
  previewCanvas.id = 'preview-canvas';
  previewCanvas.style.display = 'block';
  previewCanvas.style.cursor = 'crosshair';
  wrapper.appendChild(previewCanvas);
  previewCtx = previewCanvas.getContext('2d');

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
  document.addEventListener('mouseup',  onDocMouseUp);
}

// ============================================================
// File System
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
  for (const k of Array.from(nodeFiles.keys()).sort()) {
    const opt = document.createElement('option');
    opt.value = k;
    opt.textContent = k;
    selNode.appendChild(opt);
  }
}

// ============================================================
// Data Normalisation
// Converts legacy flat arrays to tabs format for editing.
// ============================================================
function normaliseData(data) {
  // Compact
  if (!data.tabs) {
    data.tabs = [{ label: '', elements: data.elements || [] }];
    delete data.elements;
  }
  data.tabs.forEach(t => { if (!t.elements) t.elements = []; });

  // Unfolded
  if (!data.unfoldedTabs) {
    const src = data.unfoldedElements || data.extendedElements || [];
    data.unfoldedTabs = [{ label: '', elements: src }];
    delete data.unfoldedElements;
    delete data.extendedElements;
  }
  data.unfoldedTabs.forEach(t => { if (!t.elements) t.elements = []; });

  // Unfold extents
  if (!data.unfoldExtents) {
    data.unfoldExtents = { up: 0, down: 0, left: 0, right: 0 };
  }

  return data;
}

// Serialise to save format: single unnamed tab → flat elements array.
function serialiseForSave(data) {
  const out = {
    gridWidth:  data.gridWidth  || 1,
    gridHeight: data.gridHeight || 1,
  };

  // Compact
  if (data.tabs.length === 1 && !data.tabs[0].label) {
    out.elements = roundBounds(data.tabs[0].elements);
  } else {
    out.tabs = data.tabs.map(t => ({
      label: t.label,
      elements: roundBounds(t.elements),
    }));
  }

  // Unfold extents (omit if all 0s)
  const e = data.unfoldExtents || {};
  if ((e.up||0) || (e.down||0) || (e.left||0) || (e.right||0)) {
    out.unfoldExtents = {
      up:    e.up    || 0,
      down:  e.down  || 0,
      left:  e.left  || 0,
      right: e.right || 0,
    };
  }

  // Unfolded elements
  const hasUnfoldElements = data.unfoldedTabs && data.unfoldedTabs.some(t => t.elements.length > 0);
  if (hasUnfoldElements) {
    if (data.unfoldedTabs.length === 1 && !data.unfoldedTabs[0].label) {
      out.unfoldedElements = roundBounds(data.unfoldedTabs[0].elements);
    } else {
      out.unfoldedTabs = data.unfoldedTabs.map(t => ({
        label: t.label,
        elements: roundBounds(t.elements),
      }));
    }
  }

  return out;
}

function roundBounds(elements) {
  return elements.map(el => ({
    ...el,
    gridBounds: (el.gridBounds || [0,0,1,1]).map(v => Math.round(v)),
  }));
}

// ============================================================
// Mode / Tab Helpers
// ============================================================
function isUnfoldedMode() { return previewMode === 1; }

function getUnfoldExtents() {
  if (!currentData || !currentData.unfoldExtents) return { up:0, down:0, left:0, right:0 };
  const e = currentData.unfoldExtents;
  return { up: e.up||0, down: e.down||0, left: e.left||0, right: e.right||0 };
}

function activeTabsArray() {
  if (!currentData) return [];
  if (previewMode === 0) return currentData.tabs || [];
  return currentData.unfoldedTabs || [];
}

function activeTabIndex() {
  return previewMode === 0 ? currentCompactTab : currentUnfoldedTab;
}

function setActiveTabIndex(t) {
  if (previewMode === 0) currentCompactTab  = t;
  else currentUnfoldedTab = t;
}

// Returns the editable elements array for the current mode + active tab.
function currentElements() {
  const tabs = activeTabsArray();
  if (!tabs || tabs.length === 0) return [];
  const idx = Math.min(activeTabIndex(), tabs.length - 1);
  return (tabs[idx] || { elements: [] }).elements;
}

function getNodeGridSize() {
  if (!currentData) return { gw: 1, gh: 1 };
  const bw = currentData.gridWidth  || 1;
  const bh = currentData.gridHeight || 1;
  if (isUnfoldedMode()) {
    const e = getUnfoldExtents();
    return { gw: e.left + bw + e.right, gh: e.up + bh + e.down };
  }
  return { gw: bw, gh: bh };
}

// ============================================================
// Load / Save Handlers
// ============================================================
selNode.addEventListener('change', async (e) => {
  const key = e.target.value;
  if (!key) return;
  try {
    statusMsg.textContent = 'Loading...';
    const file = await nodeFiles.get(key).getFile();
    currentData = normaliseData(JSON.parse(await file.text()));
    currentNodeKey = key;
    selectedElementIndex = -1;
    currentCompactTab = currentUnfoldedTab = 0;
    undoStack.length = redoStack.length = 0;
    updateUndoButtons();

    document.getElementById('node-w').value = currentData.gridWidth  || 1;
    document.getElementById('node-h').value = currentData.gridHeight || 1;
    document.getElementById('sel-preview-mode').value = '0';
    previewMode = 0;

    syncUnfoldExtentsInputs();
    updateModeSettingsVisibility();

    initCanvas();
    renderAll();
    renderProperties();
    renderTabSelector();
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
    const out = serialiseForSave(currentData);
    const fileHandle = nodeFiles.get(currentNodeKey);
    const writable = await fileHandle.createWritable();
    await writable.write(JSON.stringify(out, null, 4));
    await writable.close();
    statusMsg.textContent = 'Saved successfully.';
  } catch (err) {
    console.error(err);
    statusMsg.textContent = 'Failed to save.';
  }
});

// Node size inputs
document.getElementById('node-w').addEventListener('change', (e) => {
  if (!currentData) return;
  pushUndo();
  currentData.gridWidth = parseInt(e.target.value) || 1;
  renderAll();
});
document.getElementById('node-h').addEventListener('change', (e) => {
  if (!currentData) return;
  pushUndo();
  currentData.gridHeight = parseInt(e.target.value) || 1;
  renderAll();
});

// Unfold extents inputs
['up','down','left','right'].forEach(dir => {
  document.getElementById(`unfold-${dir}`).addEventListener('change', (e) => {
    if (!currentData) return;
    pushUndo();
    if (!currentData.unfoldExtents) currentData.unfoldExtents = { up:1, down:1, left:1, right:1 };
    currentData.unfoldExtents[dir] = parseInt(e.target.value) || 1;
    renderAll();
  });
});

// Mode selector
document.getElementById('sel-preview-mode').addEventListener('change', (e) => {
  previewMode = parseInt(e.target.value);
  selectedElementIndex = -1;
  updateModeSettingsVisibility();
  renderAll();
  renderProperties();
  renderTabSelector();
});

// Scale slider
scaleSlider.addEventListener('input', () => {
  SCALE = parseFloat(scaleSlider.value);
  scaleDisplay.textContent = SCALE.toFixed(1) + '×';
  if (currentData) renderAll();
});

function syncUnfoldExtentsInputs() {
  const e = getUnfoldExtents();
  document.getElementById('unfold-up').value    = e.up;
  document.getElementById('unfold-down').value  = e.down;
  document.getElementById('unfold-left').value  = e.left;
  document.getElementById('unfold-right').value = e.right;
}

function updateModeSettingsVisibility() {
  document.getElementById('unfold-settings').style.display = isUnfoldedMode() ? '' : 'none';
}

// ============================================================
// Tab Management UI
// ============================================================
function renderTabSelector() {
  const section = document.getElementById('tab-bar-section');
  if (!section) return;

  if (!currentData) {
    section.innerHTML = '';
    return;
  }

  const tabs = activeTabsArray();
  const activeIdx = activeTabIndex();
  const modeLabel = ['Compact', 'Unfolded'][previewMode];

  let html = `<div style="margin-top:8px">
    <div class="prop-row"><label>${modeLabel} Tabs</label>
    <div style="display:flex;flex-wrap:wrap;gap:4px;align-items:center">`;

  for (let i = 0; i < tabs.length; i++) {
    const label = tabs[i].label || `Tab ${i+1}`;
    const active = i === activeIdx;
    html += `<button class="tab-sel-btn${active?' active':''}" data-tab="${i}"
      style="padding:2px 8px;font-size:11px;background:${active?'rgba(13,240,227,0.2)':'rgba(255,255,255,0.06)'};
      border:1px solid ${active?COLOR_NEON:'rgba(255,255,255,0.2)'};border-radius:3px;color:${active?COLOR_NEON:'#ccc'};cursor:pointer">
      ${label}</button>`;
  }

  html += `<button id="btn-add-tab" title="Add tab" style="padding:2px 8px;font-size:13px;font-weight:bold">+</button>`;
  if (tabs.length > 1) {
    html += `<button id="btn-del-tab" title="Delete active tab" style="padding:2px 8px;font-size:13px;font-weight:bold;color:#e88">−</button>`;
  }

  html += `</div></div>`;

  if (tabs[activeIdx]) {
    html += `<div class="prop-row"><label>Tab Label</label>
      <input type="text" id="tab-label-input" value="${tabs[activeIdx].label || ''}"></div>`;
  }

  html += `</div>`;
  section.innerHTML = html;

  section.querySelectorAll('.tab-sel-btn').forEach(btn => {
    btn.addEventListener('click', () => {
      setActiveTabIndex(parseInt(btn.dataset.tab));
      selectedElementIndex = -1;
      renderAll();
      renderProperties();
      renderTabSelector();
    });
  });

  const addBtn = document.getElementById('btn-add-tab');
  if (addBtn) {
    addBtn.addEventListener('click', () => {
      pushUndo();
      const t = activeTabsArray();
      t.push({ label: `Tab ${t.length + 1}`, elements: [] });
      setActiveTabIndex(t.length - 1);
      selectedElementIndex = -1;
      renderAll();
      renderTabSelector();
    });
  }

  const delBtn = document.getElementById('btn-del-tab');
  if (delBtn) {
    delBtn.addEventListener('click', () => {
      const t = activeTabsArray();
      if (t.length <= 1) return;
      const name = t[activeTabIndex()].label || `Tab ${activeTabIndex()+1}`;
      if (!confirm(`Delete "${name}"? All elements in it will be lost.`)) return;
      pushUndo();
      t.splice(activeTabIndex(), 1);
      setActiveTabIndex(Math.min(activeTabIndex(), t.length - 1));
      selectedElementIndex = -1;
      renderAll();
      renderProperties();
      renderTabSelector();
    });
  }

  const labelInput = document.getElementById('tab-label-input');
  if (labelInput) {
    labelInput.addEventListener('change', (ev) => {
      pushUndo();
      activeTabsArray()[activeTabIndex()].label = ev.target.value;
      renderAll();
      renderTabSelector();
    });
  }
}

// ============================================================
// Geometry Helpers
// ============================================================
function nodePixelSize(gw, gh) {
  return {
    w: (gw * GRID_PITCH - TRAMLINE_MARGIN) * SCALE,
    h: (gh * GRID_PITCH - TRAMLINE_MARGIN) * SCALE,
  };
}

// Body region where elements are laid out for the current mode.
// Accounts for TAB_HEIGHT when mode has ≥2 tabs.
function bodyRegionForMode(nodeW, nodeH) {
  const tabs = activeTabsArray();
  const tabBar = tabs.length > 1 ? TAB_HEIGHT * SCALE : 0;
  return {
    x: PORT_MARGIN * SCALE,
    y: HEADER_HEIGHT * SCALE + tabBar,
    w: nodeW - PORT_MARGIN * 2 * SCALE,
    h: nodeH - HEADER_HEIGHT * SCALE - tabBar - BOTTOM_PAD * SCALE,
  };
}

// Compact ghost body shown dimmed inside the unfolded panel.
function compactGhostBodyRegion() {
  const e = getUnfoldExtents();
  const bw = currentData.gridWidth  || 1;
  const bh = currentData.gridHeight || 1;
  const cpx = e.left * GRID_PITCH * SCALE;
  const cpy = e.up   * GRID_PITCH * SCALE;
  const cpw = (bw * GRID_PITCH - TRAMLINE_MARGIN) * SCALE;
  const cph = (bh * GRID_PITCH - TRAMLINE_MARGIN) * SCALE;
  const compactTabs = currentData.tabs || [];
  const tabBar = compactTabs.length > 1 ? TAB_HEIGHT * SCALE : 0;
  return {
    x: cpx + PORT_MARGIN * SCALE,
    y: cpy + HEADER_HEIGHT * SCALE + tabBar,
    w: cpw - PORT_MARGIN * 2 * SCALE,
    h: cph - HEADER_HEIGHT * SCALE - tabBar - BOTTOM_PAD * SCALE,
  };
}

function subGridPitch(bodyW, bodyH, gw, gh) {
  return { sx: bodyW / (gw * SUBS_PER_UNIT), sy: bodyH / (gh * SUBS_PER_UNIT) };
}

function elementRect(el, body, sx, sy) {
  const b = el.gridBounds;
  return { x: body.x + b[0]*sx, y: body.y + b[1]*sy, w: b[2]*sx, h: b[3]*sy };
}

// ============================================================
// Main Render
// ============================================================
function renderAll() {
  if (!currentData || !previewCtx) return;

  const bw = currentData.gridWidth  || 1;
  const bh = currentData.gridHeight || 1;
  const { gw, gh } = getNodeGridSize();
  const { w: nodeW, h: nodeH } = nodePixelSize(gw, gh);

  previewCanvas.width  = nodeW;
  previewCanvas.height = nodeH;

  const ctx = previewCtx;
  const unfolded = isUnfoldedMode();

  drawNodeBody(ctx, nodeW, nodeH, unfolded, bw, bh);

  const body = bodyRegionForMode(nodeW, nodeH);
  const { sx, sy } = subGridPitch(body.w, body.h, gw, gh);
  drawSubGrid(ctx, body, gw, gh, sx, sy);

  const editElems = currentElements();
  editElems.forEach((el, i) => {
    drawElement(ctx, el, elementRect(el, body, sx, sy), i === selectedElementIndex);
  });

  if (selectedElementIndex >= 0 && selectedElementIndex < editElems.length) {
    const r = elementRect(editElems[selectedElementIndex], body, sx, sy);
    ctx.strokeStyle = COLOR_NEON;
    ctx.lineWidth = 1.5;
    ctx.setLineDash([4, 3]);
    ctx.strokeRect(r.x + 0.5, r.y + 0.5, r.w - 1, r.h - 1);
    ctx.setLineDash([]);
  }

  renderOverlay(body, sx, sy, editElems);

  const badge = document.getElementById('edit-mode-badge');
  if (badge) {
    const modeLabel = unfolded ? 'Unfolded' : 'Compact';
    const tabs = activeTabsArray();
    const tabSuffix = tabs.length > 1
      ? ` — ${tabs[activeTabIndex()]?.label || 'Tab '+(activeTabIndex()+1)}`
      : '';
    badge.textContent = `Editing: ${modeLabel}${tabSuffix}`;
    badge.className = `mode-badge${unfolded ? ' mode-badge--extended' : ''}`;
  }
}

// ============================================================
// Node Body Draw
// ============================================================
function drawNodeBody(ctx, nodeW, nodeH, unfolded, baseGw, baseGh) {
  const R  = 6 * (SCALE / 3);
  const Ru = 8 * (SCALE / 3);

  if (unfolded) {
    ctx.fillStyle = COLOR_BG_NODE_UNFOLDED;
    roundRect(ctx, 0, 0, nodeW, nodeH, Ru); ctx.fill();
    ctx.strokeStyle = COLOR_NEON_DIM;
    ctx.lineWidth = 1.5;
    roundRect(ctx, 0.5, 0.5, nodeW-1, nodeH-1, Ru); ctx.stroke();
  } else {
    ctx.fillStyle = COLOR_BG_NODE;
    roundRect(ctx, 0, 0, nodeW, nodeH, R); ctx.fill();
    ctx.strokeStyle = COLOR_NEON_DIM;
    ctx.lineWidth = 1.5;
    roundRect(ctx, 0.5, 0.5, nodeW-1, nodeH-1, R); ctx.stroke();
  }

  // Header divider
  ctx.strokeStyle = 'rgba(13, 240, 227, 0.2)';
  ctx.lineWidth = 1;
  ctx.beginPath();
  ctx.moveTo(0, HEADER_HEIGHT * SCALE);
  ctx.lineTo(nodeW, HEADER_HEIGHT * SCALE);
  ctx.stroke();

  // Tab bar
  const activeTabs = activeTabsArray();
  if (activeTabs && activeTabs.length > 1) {
    drawTabBar(ctx, nodeW, activeTabs, activeTabIndex());
  }

  // Compact tab bar hint inside unfolded ghost
  if (unfolded) {
    const compactTabs = currentData.tabs || [];
    if (compactTabs.length > 1) {
      const e = getUnfoldExtents();
      const cpx = e.left * GRID_PITCH * SCALE;
      const cpy = e.up   * GRID_PITCH * SCALE;
      const cpw = (baseGw * GRID_PITCH - TRAMLINE_MARGIN) * SCALE;
      drawTabBarHint(ctx, cpx, cpy + HEADER_HEIGHT * SCALE, cpw, compactTabs, currentCompactTab);
    }
  }

  // Ports
  drawPort(ctx, 0,     HEADER_HEIGHT * SCALE + 10 * SCALE, PORT_RADIUS * SCALE, true);
  drawPort(ctx, nodeW, HEADER_HEIGHT * SCALE + 10 * SCALE, PORT_RADIUS * SCALE, false);

  // Title
  const nodeName = currentNodeKey
    ? currentNodeKey.split('/').pop().replace('.json', '').replace('Node', ' Node')
    : 'Node';
  ctx.fillStyle = COLOR_TEXT;
  ctx.font = `bold ${Math.max(9, 14 * SCALE / 3)}px "Segoe UI", sans-serif`;
  ctx.textAlign = 'left';
  ctx.textBaseline = 'middle';
  ctx.fillText(nodeName, 8 * SCALE / 3, HEADER_HEIGHT * SCALE / 2);

  // Delete button placeholder
  const btnSize = (HEADER_HEIGHT - 6) * SCALE;
  const btnX = nodeW - btnSize - 3 * (SCALE / 3);
  const btnY = 3 * (SCALE / 3);
  ctx.fillStyle = 'rgba(0,0,0,0.2)';
  roundRect(ctx, btnX, btnY, btnSize, btnSize, 3); ctx.fill();
  ctx.fillStyle = 'rgba(255,255,255,0.5)';
  ctx.font = `${Math.max(7, 10 * SCALE / 3)}px sans-serif`;
  ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
  ctx.fillText('×', btnX + btnSize/2, btnY + btnSize/2);

  // Expand button (shown when has unfolded content)
  const hasExpand = currentData.unfoldedTabs?.some(t => t.elements.length > 0);
  if (hasExpand) {
    const gap = 4 * (SCALE / 3);
    const expBtnX = btnX - btnSize - gap;
    ctx.fillStyle = 'rgba(13, 240, 227, 0.12)';
    roundRect(ctx, expBtnX, btnY, btnSize, btnSize, 3); ctx.fill();
    ctx.strokeStyle = 'rgba(13, 240, 227, 0.5)';
    ctx.lineWidth = 1;
    roundRect(ctx, expBtnX+0.5, btnY+0.5, btnSize-1, btnSize-1, 3); ctx.stroke();
    ctx.fillStyle = unfolded ? COLOR_NEON : 'rgba(13, 240, 227, 0.7)';
    ctx.font = `${Math.max(7, 10 * SCALE / 3)}px sans-serif`;
    ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
    ctx.fillText(unfolded ? '<' : '>', expBtnX + btnSize/2, btnY + btnSize/2);
  }
}

function drawTabBar(ctx, nodeW, tabs, activeIdx) {
  const barY = HEADER_HEIGHT * SCALE;
  const barH = TAB_HEIGHT * SCALE;
  const tabW = nodeW / tabs.length;

  for (let t = 0; t < tabs.length; t++) {
    const tx = t * tabW;
    const tw = (t === tabs.length - 1) ? nodeW - tx : tabW;
    if (t === activeIdx) {
      ctx.fillStyle = 'rgba(13, 240, 227, 0.2)';
      ctx.fillRect(tx + 1, barY + 1, tw - 2, barH - 2);
    }
    ctx.strokeStyle = 'rgba(13, 240, 227, 0.4)';
    ctx.lineWidth = 1;
    ctx.strokeRect(tx + 0.5, barY + 0.5, tw - 1, barH - 1);
    ctx.fillStyle = (t === activeIdx) ? '#ffffff' : 'rgba(255,255,255,0.55)';
    ctx.font = `${Math.max(6, 10 * SCALE / 3)}px "Segoe UI", sans-serif`;
    ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
    ctx.fillText(tabs[t].label || `Tab ${t+1}`, tx + tw/2, barY + barH/2);
  }
}

// Dimmed tab bar drawn over the compact ghost inside an unfolded view
function drawTabBarHint(ctx, ghostX, y, ghostW, tabs, activeIdx) {
  const barH = TAB_HEIGHT * SCALE;
  const tabW = ghostW / tabs.length;
  ctx.save();
  ctx.globalAlpha = 0.45;
  for (let t = 0; t < tabs.length; t++) {
    const tx = ghostX + t * tabW;
    const tw = (t === tabs.length - 1) ? ghostX + ghostW - tx : tabW;
    ctx.strokeStyle = 'rgba(13, 240, 227, 0.4)';
    ctx.lineWidth = 1;
    ctx.strokeRect(tx + 0.5, y + 0.5, tw - 1, barH - 1);
    ctx.fillStyle = (t === activeIdx) ? '#ffffff' : 'rgba(255,255,255,0.4)';
    ctx.font = `${Math.max(5, 9 * SCALE / 3)}px "Segoe UI", sans-serif`;
    ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
    ctx.fillText(tabs[t].label || `Tab ${t+1}`, tx + tw/2, y + barH/2);
  }
  ctx.restore();
}

function drawCompactBodyHint(ctx, baseGw, baseGh) {
  const e = getUnfoldExtents();
  const cpx = e.left * GRID_PITCH * SCALE;
  const cpy = e.up   * GRID_PITCH * SCALE;
  const cpw = (baseGw * GRID_PITCH - TRAMLINE_MARGIN) * SCALE;
  const cph = (baseGh * GRID_PITCH - TRAMLINE_MARGIN) * SCALE;
  const R = 4 * (SCALE / 3);

  ctx.save();
  ctx.strokeStyle = 'rgba(136, 119, 85, 0.5)';
  ctx.lineWidth = 1.2;
  ctx.setLineDash([5, 3]);
  roundRect(ctx, cpx+1, cpy+1, cpw-2, cph-2, R);
  ctx.stroke();
  ctx.setLineDash([]);
  ctx.restore();
}

function drawPort(ctx, edgeX, centreY, r, isInput) {
  const fillColor   = isInput ? COLOR_PORT_IN   : COLOR_PORT_OUT;
  const borderColor = isInput ? COLOR_PORT_IN_BD : COLOR_PORT_OUT_BD;

  ctx.save();
  ctx.beginPath();
  if (isInput) ctx.rect(edgeX, centreY - r, r, r * 2);
  else         ctx.rect(edgeX - r, centreY - r, r, r * 2);
  ctx.clip();

  ctx.beginPath();
  ctx.roundRect(isInput ? edgeX - r : edgeX - r, centreY - r, r*2, r*2, r);
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
    ctx.beginPath(); ctx.moveTo(x, body.y); ctx.lineTo(x, body.y + body.h); ctx.stroke();
  }
  for (let r = 0; r <= rows; r++) {
    const y = body.y + r * sy;
    ctx.beginPath(); ctx.moveTo(body.x, y); ctx.lineTo(body.x + body.w, y); ctx.stroke();
  }
}

function drawElement(ctx, el, r, selected) {
  switch (el.type) {
    case 'RotarySlider': drawRotary(ctx, el, r); break;
    case 'Label':        drawLabel(ctx, el, r);  break;
    case 'Toggle':
    case 'PushButton':   drawToggle(ctx, el, r); break;
    case 'ComboBox':     drawCombo(ctx, el, r);  break;
    default:             drawCustom(ctx, el, r); break;
  }
}

// ============================================================
// Element Drawers
// ============================================================
function drawRotary(ctx, el, r) {
  const cx = r.x + r.w/2, cy = r.y + r.h/2;
  const radius = (Math.min(r.w, r.h)/2) - 2;
  const trackWidth = radius * 0.4;
  const sliderPos = 0.5;
  const angle = ROTARY_START + sliderPos * (ROTARY_END - ROTARY_START);

  ctx.fillStyle = 'rgba(0,0,0,0.1)';
  ctx.beginPath(); ctx.ellipse(cx, cy, radius, radius, 0, 0, Math.PI*2); ctx.fill();

  ctx.beginPath(); ctx.arc(cx, cy, radius, ROTARY_START, ROTARY_END);
  ctx.strokeStyle = COLOR_TRACK_DARK; ctx.lineWidth = trackWidth; ctx.lineCap = 'round'; ctx.stroke();

  ctx.beginPath(); ctx.arc(cx, cy, radius, ROTARY_START, angle);
  ctx.strokeStyle = 'rgba(13, 240, 227, 0.2)'; ctx.lineWidth = trackWidth * 1.5; ctx.stroke();

  ctx.beginPath(); ctx.arc(cx, cy, radius, ROTARY_START, angle);
  ctx.strokeStyle = COLOR_NEON; ctx.lineWidth = trackWidth; ctx.stroke();

  const fontSize = Math.max(6, radius * 0.55);
  const midVal = el.floatMin !== undefined
    ? ((el.floatMin + el.floatMax) / 2).toFixed(2)
    : String(Math.round(((el.minValue ?? 0) + (el.maxValue ?? 1)) / 2));
  ctx.fillStyle = COLOR_TEXT;
  ctx.font = `bold ${fontSize}px "Segoe UI", sans-serif`;
  ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
  ctx.fillText(midVal, cx, cy + 1);

  const dotX = cx + radius * Math.cos(angle - Math.PI/2);
  const dotY = cy + radius * Math.sin(angle - Math.PI/2);
  ctx.fillStyle = COLOR_TEXT;
  ctx.beginPath(); ctx.arc(dotX, dotY, 3, 0, Math.PI*2); ctx.fill();

  if (el.bipolar) {
    const ca = (ROTARY_START + ROTARY_END)/2 - Math.PI/2;
    const ti = radius - trackWidth * 0.8;
    const to = radius + trackWidth * 0.5;
    ctx.strokeStyle = 'rgba(255,255,255,0.5)'; ctx.lineWidth = Math.max(1, trackWidth*0.3); ctx.lineCap = 'round';
    ctx.beginPath();
    ctx.moveTo(cx + ti*Math.cos(ca), cy + ti*Math.sin(ca));
    ctx.lineTo(cx + to*Math.cos(ca), cy + to*Math.sin(ca));
    ctx.stroke();
  }
}

function drawLabel(ctx, el, r) {
  let color = COLOR_LABEL, bold = false;
  if (el.colorHex) {
    const hex = el.colorHex.length === 8 ? el.colorHex.substring(2) : el.colorHex;
    color = '#' + hex; bold = true;
  }
  ctx.fillStyle = color;
  ctx.font = `${bold ? 'bold ' : ''}${Math.max(6, 13 * SCALE / 3)}px "Segoe UI", sans-serif`;
  ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
  ctx.save();
  ctx.rect(r.x, r.y, r.w, r.h); ctx.clip();
  ctx.fillText(el.label || '', r.x + r.w/2, r.y + r.h/2);
  ctx.restore();
}

function drawToggle(ctx, el, r) {
  const R = 4 * SCALE / 3;
  ctx.fillStyle = COLOR_BG_NODE;
  roundRect(ctx, r.x, r.y, r.w, r.h, R); ctx.fill();
  ctx.strokeStyle = COLOR_NEON_DIM; ctx.lineWidth = 1;
  roundRect(ctx, r.x+0.5, r.y+0.5, r.w-1, r.h-1, R); ctx.stroke();
  ctx.fillStyle = COLOR_TEXT;
  ctx.font = `bold ${Math.max(6, 11 * SCALE)}px "Segoe UI", sans-serif`;
  ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
  ctx.save(); ctx.rect(r.x, r.y, r.w, r.h); ctx.clip();
  ctx.fillText(el.label || '', r.x + r.w/2, r.y + r.h/2);
  ctx.restore();
}

function drawCombo(ctx, el, r) {
  const R = 2 * SCALE / 3;
  ctx.fillStyle = COLOR_COMBO_BG;
  roundRect(ctx, r.x, r.y, r.w, r.h, R); ctx.fill();
  ctx.strokeStyle = COLOR_COMBO_BORDER; ctx.lineWidth = 1;
  roundRect(ctx, r.x+0.5, r.y+0.5, r.w-1, r.h-1, R); ctx.stroke();

  const displayText = (el.options && el.options.length > 0) ? el.options[0] : (el.label || '');
  ctx.fillStyle = COLOR_TEXT;
  ctx.font = `${Math.max(6, 11 * SCALE)}px "Segoe UI", sans-serif`;
  ctx.textAlign = 'left'; ctx.textBaseline = 'middle';
  ctx.save(); ctx.rect(r.x, r.y, r.w, r.h); ctx.clip();
  ctx.fillText(displayText, r.x + 4 * SCALE / 3, r.y + r.h/2);
  ctx.restore();

  const arrowSize = Math.max(4, r.h * 0.4);
  const ax = r.x + r.w - arrowSize - 3 * SCALE / 3, ay = r.y + r.h/2;
  ctx.fillStyle = COLOR_NEON;
  ctx.beginPath();
  ctx.moveTo(ax, ay - arrowSize*0.3);
  ctx.lineTo(ax + arrowSize, ay - arrowSize*0.3);
  ctx.lineTo(ax + arrowSize/2, ay + arrowSize*0.4);
  ctx.closePath(); ctx.fill();
}

function drawCustom(ctx, el, r) {
  ctx.fillStyle = 'rgba(80, 80, 80, 0.4)';
  ctx.strokeStyle = 'rgba(140, 140, 140, 0.6)';
  ctx.lineWidth = 1;
  ctx.setLineDash([4, 3]);
  ctx.strokeRect(r.x+0.5, r.y+0.5, r.w-1, r.h-1);
  ctx.setLineDash([]);
  ctx.fillStyle = 'rgba(180,180,180,0.6)';
  ctx.font = `italic ${Math.max(6, 11 * SCALE)}px "Segoe UI", sans-serif`;
  ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
  ctx.fillText(el.customType || el.type || 'Custom', r.x + r.w/2, r.y + r.h/2);
}

// ============================================================
// Interaction Overlay
// ============================================================
let isDragging = false, isResizing = false;
let dragStartX = 0, dragStartY = 0;
let dragStartBounds = null;
let interactIdx = -1;

function getBodyAndPitch() {
  if (!currentData) return null;
  const { gw, gh } = getNodeGridSize();
  const { w: nw, h: nh } = nodePixelSize(gw, gh);
  const body = bodyRegionForMode(nw, nh);
  const { sx, sy } = subGridPitch(body.w, body.h, gw, gh);
  return { body, sx, sy };
}

function renderOverlay(body, sx, sy, editElements) {
  overlayDiv.innerHTML = '';
  overlayDiv.style.width  = previewCanvas.width  + 'px';
  overlayDiv.style.height = previewCanvas.height + 'px';
  overlayDiv.style.left   = previewCanvas.offsetLeft + 'px';
  overlayDiv.style.top    = previewCanvas.offsetTop  + 'px';

  editElements.forEach((el, index) => {
    const r = elementRect(el, body, sx, sy);
    const handle = document.createElement('div');
    handle.className = 'overlay-element';
    handle.style.left = r.x + 'px'; handle.style.top  = r.y + 'px';
    handle.style.width = r.w + 'px'; handle.style.height = r.h + 'px';
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

function elementAtPoint(px, py) {
  const g = getBodyAndPitch();
  if (!g) return -1;
  const { body, sx, sy } = g;
  const elements = currentElements();
  for (let i = elements.length - 1; i >= 0; i--) {
    const r = elementRect(elements[i], body, sx, sy);
    if (px >= r.x && px <= r.x+r.w && py >= r.y && py <= r.y+r.h) return i;
  }
  return -1;
}

function onCanvasMouseDown(e) {
  const rect = previewCanvas.getBoundingClientRect();
  const px = e.clientX - rect.left, py = e.clientY - rect.top;
  const idx = elementAtPoint(px, py);

  if (idx === -1) {
    selectedElementIndex = -1;
    renderAll(); renderProperties();
    return;
  }

  const elements = currentElements();
  selectedElementIndex = idx;
  interactIdx = idx;
  dragStartX = e.clientX; dragStartY = e.clientY;
  dragStartBounds = [...elements[idx].gridBounds];
  pushUndo();

  const g = getBodyAndPitch();
  const r = elementRect(elements[idx], g.body, g.sx, g.sy);
  isResizing = (px > r.x + r.w - 10) && (py > r.y + r.h - 10);
  isDragging = !isResizing;

  renderAll(); renderProperties();
  e.preventDefault();
}

function onCanvasMouseMove(e) {
  if (isDragging || isResizing) return;
  const rect = previewCanvas.getBoundingClientRect();
  const px = e.clientX - rect.left, py = e.clientY - rect.top;
  const g = getBodyAndPitch();
  if (!g) return;
  const idx = elementAtPoint(px, py);
  if (idx !== -1) {
    const r = elementRect(currentElements()[idx], g.body, g.sx, g.sy);
    previewCanvas.style.cursor = (px > r.x+r.w-10 && py > r.y+r.h-10) ? 'se-resize' : 'move';
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
  const dx = e.clientX - dragStartX, dy = e.clientY - dragStartY;
  const gDx = Math.round(dx / sx), gDy = Math.round(dy / sy);
  const b = currentElements()[interactIdx].gridBounds;
  if (isDragging) {
    b[0] = Math.max(0, dragStartBounds[0] + gDx);
    b[1] = Math.max(0, dragStartBounds[1] + gDy);
  } else if (isResizing) {
    b[2] = Math.max(1, dragStartBounds[2] + gDx);
    b[3] = Math.max(1, dragStartBounds[3] + gDy);
  }
  renderAll();
}

function onDocMouseUp() {
  if (isDragging || isResizing) renderProperties();
  isDragging = isResizing = false;
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
    const modeLabel = ['Compact', 'Tab-Expanded', 'Unfolded'][previewMode];
    propsContent.innerHTML = `<p>Select an element to edit, or use the Add button below.</p>
      ${currentData ? `
      <div style="margin-top:12px">
        <div class="prop-row">
          <label>Add to ${modeLabel}</label>
          <div style="display:flex;gap:6px;align-items:center">
            <select id="new-el-type" style="flex:1;min-width:0">
              ${Object.keys(ELEMENT_DEFAULTS).map(t => `<option value="${t}">${t}</option>`).join('')}
            </select>
            <button id="btn-add-element" style="white-space:nowrap">+ Add</button>
          </div>
        </div>
      </div>` : ''}`;

    if (currentData) {
      document.getElementById('btn-add-element').addEventListener('click', () => {
        const type = document.getElementById('new-el-type').value;
        pushUndo();
        const newEl = JSON.parse(JSON.stringify(ELEMENT_DEFAULTS[type]));
        currentElements().push(newEl);
        selectedElementIndex = currentElements().length - 1;
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
      <label>Type</label><input type="text" value="${el.type}" disabled>
    </div>
    ${createInput('label', 'Label', el.label || '')}
    ${createBoundsInputs(el.gridBounds)}`;

  if (el.type === 'RotarySlider') {
    html += createInput('minValue', 'Min Value', el.minValue !== undefined ? el.minValue : 0, 'number');
    html += createInput('maxValue', 'Max Value', el.maxValue !== undefined ? el.maxValue : 1, 'number');
    html += createInput('floatMin', 'Float Min', el.floatMin !== undefined ? el.floatMin : '', 'number', 'any');
    html += createInput('floatMax', 'Float Max', el.floatMax !== undefined ? el.floatMax : '', 'number', 'any');
    html += createInput('step', 'Step', el.step !== undefined ? el.step : '', 'number', 'any');
    html += createCheckbox('bipolar', 'Bipolar', el.bipolar);
  }
  if (el.type === 'Label') {
    html += createInput('colorHex', 'Color Hex (aarrggbb)', el.colorHex || '');
  }
  if (el.type === 'ComboBox') {
    html += `<div class="prop-row"><label>Options (one per line)</label>
      <textarea id="combo-options" rows="5" style="background:#131820;border:1px solid #2e3440;color:white;
      padding:5px 7px;border-radius:3px;font-size:12px;resize:vertical;width:100%">${(el.options||[]).join('\n')}</textarea></div>`;
  }
  if (el.type === 'Custom') {
    html += createInput('customType', 'Custom Class ID', el.customType || '');
  }

  html += `<button id="btn-delete-element" style="background-color:#c93434;margin-top:15px;width:100%">Delete Element</button>`;
  propsContent.innerHTML = html;

  if (el.type === 'ComboBox') {
    document.getElementById('combo-options').addEventListener('change', (e) => {
      pushUndo();
      el.options = e.target.value.split('\n').map(s => s.trim()).filter(s => s);
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
      if (val === '' || (typeof val === 'number' && isNaN(val))) delete el[key];
      else el[key] = val;
      renderAll();
    });
  });

  propsContent.querySelectorAll('.dyn-bound').forEach(inp => {
    inp.addEventListener('change', (e) => {
      pushUndo();
      el.gridBounds[parseInt(e.target.dataset.idx)] = parseInt(e.target.value) || 0;
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
  return `<div class="prop-row"><label>${title}</label>
    <input type="${type}" ${step ? `step="${step}"` : ''} class="dyn-prop" data-key="${key}" value="${val}"></div>`;
}

function createCheckbox(key, title, checked) {
  return `<div class="prop-row"><label>${title}</label>
    <input type="checkbox" class="dyn-prop" data-key="${key}" ${checked ? 'checked' : ''}></div>`;
}

function createBoundsInputs(bounds) {
  if (!bounds || bounds.length !== 4) return '';
  return `<div class="prop-row"><label>X, Y, W, H (grid subs)</label>
    <div style="display:flex;gap:5px;">
      <input type="number" class="dyn-bound" data-idx="0" value="${bounds[0]}" style="width:44px">
      <input type="number" class="dyn-bound" data-idx="1" value="${bounds[1]}" style="width:44px">
      <input type="number" class="dyn-bound" data-idx="2" value="${bounds[2]}" style="width:44px">
      <input type="number" class="dyn-bound" data-idx="3" value="${bounds[3]}" style="width:44px">
    </div></div>`;
}

// ============================================================
// Keyboard Shortcuts
// ============================================================
document.addEventListener('keydown', (e) => {
  if (['INPUT', 'SELECT', 'TEXTAREA'].includes(document.activeElement.tagName)) return;

  if (e.ctrlKey && !e.shiftKey && e.key === 'z') { undo(); e.preventDefault(); return; }
  if (e.ctrlKey && (e.key === 'y' || (e.shiftKey && e.key === 'Z'))) { redo(); e.preventDefault(); return; }

  if ((e.key === 'Delete' || e.key === 'Backspace') && selectedElementIndex !== -1 && currentData) {
    if (confirm('Delete this element?')) {
      pushUndo();
      currentElements().splice(selectedElementIndex, 1);
      selectedElementIndex = -1;
      renderAll(); renderProperties();
    }
    e.preventDefault();
  }
  if (e.key === 'Escape') {
    selectedElementIndex = -1;
    renderAll(); renderProperties();
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
