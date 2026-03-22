const SUB_PX = 15;
const SUBS_PER_UNIT = 20;

let dirHandle = null;
let nodeFiles = new Map(); // key: path "MultiplyNode/MultiplyNode.json", value: FileSystemFileHandle
let currentNodeKey = null;
let currentData = null;
let selectedElementIndex = -1;

const btnOpen = document.getElementById('btn-open-dir');
const selNode = document.getElementById('sel-node');
const btnSave = document.getElementById('btn-save');
const statusMsg = document.getElementById('status-message');
const canvas = document.getElementById('canvas');
const propsContent = document.getElementById('props-content');

// --- File System Logic ---

btnOpen.addEventListener('click', async () => {
    try {
        dirHandle = await window.showDirectoryPicker();
        statusMsg.textContent = "Scanning directory...";
        nodeFiles.clear();
        await scanDirectory(dirHandle, "");
        populateDropdown();
        statusMsg.textContent = `Found ${nodeFiles.size} node JSON files.`;
        selNode.disabled = false;
    } catch (err) {
        console.error(err);
        statusMsg.textContent = "Failed to open directory.";
    }
});

async function scanDirectory(handle, path) {
    for await (const entry of handle.values()) {
        const fullPath = path ? `${path}/${entry.name}` : entry.name;
        if (entry.kind === 'directory') {
            await scanDirectory(entry, fullPath);
        } else if (entry.kind === 'file' && entry.name.endsWith('.json')) {
            // Check if it's a "*Node.json" inside "*Node" directory
            const parts = fullPath.split('/');
            if (parts.length >= 2 && parts[parts.length - 2] === entry.name.replace('.json', '')) {
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
        statusMsg.textContent = "Loading...";
        const fileHandle = nodeFiles.get(key);
        const file = await fileHandle.getFile();
        const text = await file.text();
        currentData = JSON.parse(text);
        currentNodeKey = key;
        selectedElementIndex = -1;

        // Setup grid inputs
        document.getElementById('node-w').value = currentData.gridWidth || 1;
        document.getElementById('node-h').value = currentData.gridHeight || 1;

        renderCanvas();
        renderProperties();
        statusMsg.textContent = "Loaded " + key;
        btnSave.disabled = false;
    } catch (err) {
        console.error(err);
        statusMsg.textContent = "Error loading file.";
    }
});

btnSave.addEventListener('click', async () => {
    if (!currentData || !currentNodeKey) return;
    try {
        statusMsg.textContent = "Saving...";

        // Ensure bounds are ints
        currentData.elements.forEach(el => {
            el.gridBounds = el.gridBounds.map(v => Math.round(v));
        });

        const jsonStr = JSON.stringify(currentData, null, 4);
        const fileHandle = nodeFiles.get(currentNodeKey);
        const writable = await fileHandle.createWritable();
        await writable.write(jsonStr);
        await writable.close();
        statusMsg.textContent = "Saved to base source successfully.";
    } catch (err) {
        console.error(err);
        statusMsg.textContent = "Failed to save.";
    }
});

// --- Grid Settings ---
document.getElementById('node-w').addEventListener('change', (e) => {
    if (currentData) { currentData.gridWidth = parseInt(e.target.value); renderCanvas(); }
});
document.getElementById('node-h').addEventListener('change', (e) => {
    if (currentData) { currentData.gridHeight = parseInt(e.target.value); renderCanvas(); }
});

// --- Canvas Rendering & Interaction ---

function renderCanvas() {
    canvas.innerHTML = '';
    if (!currentData) return;

    const gw = currentData.gridWidth || 1;
    const gh = currentData.gridHeight || 1;

    canvas.style.width = (gw * SUBS_PER_UNIT * SUB_PX) + 'px';
    canvas.style.height = (gh * SUBS_PER_UNIT * SUB_PX) + 'px';

    currentData.elements.forEach((el, index) => {
        const div = document.createElement('div');
        div.className = `ui-element ui-${el.type}`;
        if (index === selectedElementIndex) div.classList.add('selected');

        const baseClass = `ui-${el.type}`;
        div.classList.add(baseClass);

        if (el.label && el.type !== 'Custom') {
            const span = document.createElement('span');
            span.textContent = el.label;
            div.appendChild(span);
        } else if (el.type === 'Custom') {
            div.textContent = el.customType || 'Custom';
        }

        // Layout bounds bounds[x, y, w, h]
        const b = el.gridBounds;
        if (b && b.length === 4) {
            div.style.left = (b[0] * SUB_PX) + 'px';
            div.style.top = (b[1] * SUB_PX) + 'px';
            div.style.width = (b[2] * SUB_PX) + 'px';
            div.style.height = (b[3] * SUB_PX) + 'px';
        }

        if (el.colorHex && el.type === 'Label') {
            div.style.color = '#' + el.colorHex.substring(2); // Strip alpha if aarrggbb
            div.style.fontWeight = 'bold';
        }

        // Resize handle
        const handle = document.createElement('div');
        handle.className = 'resize-handle';
        div.appendChild(handle);

        // Drag logic
        attachInteraction(div, handle, index);
        canvas.appendChild(div);
    });
}

function attachInteraction(elementDiv, handleDiv, index) {
    let isDragging = false;
    let isResizing = false;
    let startX, startY;
    let startBounds;

    elementDiv.addEventListener('mousedown', (e) => {
        e.stopPropagation();

        if (selectedElementIndex !== index) {
            selectedElementIndex = index;

            // Visually update selection without rebuilding the DOM!
            const allElements = canvas.querySelectorAll('.ui-element');
            allElements.forEach((div, i) => {
                if (i === index) div.classList.add('selected');
                else div.classList.remove('selected');
            });

            renderProperties();
        }

        if (e.target === handleDiv) {
            isResizing = true;
        } else {
            isDragging = true;
        }

        startX = e.clientX;
        startY = e.clientY;
        startBounds = [...currentData.elements[index].gridBounds];

        document.addEventListener('mousemove', onMouseMove);
        document.addEventListener('mouseup', onMouseUp);
    });

    function onMouseMove(e) {
        const dx = e.clientX - startX;
        const dy = e.clientY - startY;
        const gridDx = Math.round(dx / SUB_PX);
        const gridDy = Math.round(dy / SUB_PX);

        const b = currentData.elements[index].gridBounds;

        if (isDragging) {
            b[0] = Math.max(0, startBounds[0] + gridDx);
            b[1] = Math.max(0, startBounds[1] + gridDy);
        } else if (isResizing) {
            b[2] = Math.max(1, startBounds[2] + gridDx);
            b[3] = Math.max(1, startBounds[3] + gridDy);
        }

        // Real-time update
        elementDiv.style.left = (b[0] * SUB_PX) + 'px';
        elementDiv.style.top = (b[1] * SUB_PX) + 'px';
        elementDiv.style.width = (b[2] * SUB_PX) + 'px';
        elementDiv.style.height = (b[3] * SUB_PX) + 'px';
    }

    function onMouseUp(e) {
        document.removeEventListener('mousemove', onMouseMove);
        document.removeEventListener('mouseup', onMouseUp);
        isDragging = false;
        isResizing = false;
        renderProperties(); // Update bounds text
    }
}

canvas.addEventListener('mousedown', () => {
    selectedElementIndex = -1;
    renderCanvas();
    renderProperties();
});

// --- Properties Panel ---

function renderProperties() {
    if (selectedElementIndex === -1 || !currentData) {
        propsContent.innerHTML = '<p>Select an element on the canvas to edit its properties.</p>';
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
        html += createInput('minValue', 'Min Value', el.minValue !== undefined ? el.minValue : 0, 'number');
        html += createInput('maxValue', 'Max Value', el.maxValue !== undefined ? el.maxValue : 1, 'number');
        html += createInput('floatMin', 'Float Min', el.floatMin !== undefined ? el.floatMin : '', 'number', 'any');
        html += createInput('floatMax', 'Float Max', el.floatMax !== undefined ? el.floatMax : '', 'number', 'any');
        html += createInput('step', 'Step', el.step !== undefined ? el.step : '', 'number', 'any');
        html += createCheckbox('bipolar', 'Bipolar (Centered Zero)', el.bipolar);
    }

    if (el.type === 'Label') {
        html += createInput('colorHex', 'Text Color Hex (e.g. ffff00ff)', el.colorHex || '');
    }

    if (el.type === 'Custom') {
        html += createInput('customType', 'Custom Class ID', el.customType || '');
    }

    // Add delete button
    html += `<button id="btn-delete-element" style="background-color: #c93434; margin-top: 15px; width: 100%;">Delete Element</button>`;

    propsContent.innerHTML = html;

    // Attach listeners
    propsContent.querySelectorAll('.dyn-prop').forEach(inp => {
        inp.addEventListener('change', (e) => {
            const key = e.target.dataset.key;
            let val = e.target.value;
            if (e.target.type === 'number') val = parseFloat(val);
            if (e.target.type === 'checkbox') val = e.target.checked;

            if (val === '' || isNaN(val) && e.target.type === 'number') delete el[key];
            else el[key] = val;

            renderCanvas();
        });
    });

    propsContent.querySelectorAll('.dyn-bound').forEach(inp => {
        inp.addEventListener('change', (e) => {
            const idx = parseInt(e.target.dataset.idx);
            el.gridBounds[idx] = parseInt(e.target.value) || 0;
            renderCanvas();
        });
    });

    document.getElementById('btn-delete-element').addEventListener('click', () => {
        if (confirm("Delete this element?")) {
            currentData.elements.splice(selectedElementIndex, 1);
            selectedElementIndex = -1;
            renderCanvas();
            renderProperties();
        }
    });
}

function createInput(key, title, val, type = "text", step = "") {
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
        <label>X, Y, Width, Height (Grid Units)</label>
        <div style="display:flex; gap: 5px;">
            <input type="number" class="dyn-bound" data-idx="0" value="${bounds[0]}" style="width: 40px">
            <input type="number" class="dyn-bound" data-idx="1" value="${bounds[1]}" style="width: 40px">
            <input type="number" class="dyn-bound" data-idx="2" value="${bounds[2]}" style="width: 40px">
            <input type="number" class="dyn-bound" data-idx="3" value="${bounds[3]}" style="width: 40px">
        </div>
    </div>`;
}
