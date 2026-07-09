
document.addEventListener('DOMContentLoaded', function () {

  // ── fetch palette ──────────────────────────────────────────
  const hexToRgb = h => { const n = parseInt(h.slice(1), 16); return [(n >> 16) & 255, (n >> 8) & 255, n & 255] }
  let palette = []   // pico32 as [[r,g,b], …]; used by the swatches + the aseprite exporter
  const paletteReady = fetch('/palettes/pico32.json')
    .then(r => r.json())
    .then(res => {
      palette = res.palette.map(hexToRgb)
      for (let i = 0; i < res.palette.length; i++) {
        const d = document.createElement('div')
        d.style.backgroundColor = res.palette[i]
        swatches.appendChild(d)
        if (i === 8) d.classList.add('active')
      }
    })

  // ── dom refs ───────────────────────────────────────────────
  const canvasContainer = document.querySelector('.canvascontainer')
  const canvasOverlay   = document.querySelector('#sprite-canvas-overlay')
  const spriteCanvas    = document.querySelector('#sprite-canvas')
  const tilemap         = document.querySelector('#tilemap-canvas')
  const tilemapo        = document.querySelector('#tilemap-canvas-overlay')
  const swatches        = document.querySelector('.swatches')
  const framesList      = document.querySelector('.frames-list')
  const togglegroup     = document.querySelector('.actions')
  const undoButton      = document.querySelector('#undo-button')
  const redoButton      = document.querySelector('#redo-button')

  // ── settings ───────────────────────────────────────────────
  const settings = {
    cellwidth:   16,
    cellheight:  16,
    scale:       28,   // 28 × 16 = 448, matches the palette height (16 swatches × 28px)
    mapwidth:    128,   //  8 cols × 16px
    mapheight:   128,   //  8 rows × 16px  → 64 sprites total
    mapscale:    2,
    worldwidth:  16,
    worldheight: 16,
    worldscale:  16,
  }

  let selectedTilemapIndices = [0, 0]

  // the mode buttons that hold a persistent "active" highlight — undo/redo and
  // the export buttons live in the same bar but are one-shot push buttons.
  const TOOL_IDS = ['pixel', 'fill', 'selection', 'stamp', 'linetool']

  spriteCanvas.oncontextmenu = e => { e.preventDefault(); e.stopPropagation() }
  spriteCanvas.addEventListener('mousedown', mousedownCanvas)
  spriteCanvas.addEventListener('mouseup',   mouseupCanvas)
  spriteCanvas.addEventListener('mouseout',  mouseoutCanvas)
  spriteCanvas.addEventListener('mousemove', mousemoveCanvas)

  tilemap.addEventListener('mousemove', e => mousemoveTilemap(e, tilemapo.getContext('2d')))
  tilemap.addEventListener('mouseout',  e => mouseoutTilemap(e,  tilemapo.getContext('2d')))
  tilemap.addEventListener('mousedown', e => mousedownTilemap(e, tilemapo.getContext('2d')))

  document.addEventListener('mouseup',  mouseup)
  swatches.addEventListener('mousedown', mousedownSwatches)
  document.addEventListener('keydown',  keydownHandler)

  const ctx = spriteCanvas.getContext('2d')

  const tooldata = {
    id:        ctx.createImageData(1, 1),
    eraser:    ctx.createImageData(1, 1),
    rgba:      [255, 0, 77, 255],
    brushsize: 1,
    action:    'pixel',
  }

  const emptyCanvas = tilemap.toDataURL()

  let selection  = undefined
  let mousePressed      = false
  let mousePressedEvent = undefined
  let dirtyFlag  = false
  let frameIsReady = true
  let frames       = []
  let framePointer = -1
  let clipboard    = undefined
  let lineToolData = undefined
  const lineToolFunctions = ['line', 'circle', 'rectangle']
  let lineToolIndex = 0

  setToolActive('pixel')
  updateSpriteCanvasSize(ctx)
  updateTilemapSize()
  updateWorldMapSize()
  setImageData(tooldata.id, tooldata.rgba)
  createEraserImageData(tooldata.eraser)

  if (localStorage.frames) {
    frames = JSON.parse(localStorage.frames)
    // normalize older/buggy saves: every frame starts with a clean single-entry undo history
    frames.forEach(f => { f.undoStack = [f.data]; f.undoPointer = 0 })
  } else {
    addFrame()
  }

  makeFramesUI()
  gotoFrameAtIndex(framePointer + 1)
  makeLineToolUI()
  renderSelectedTileMapCell(tilemapo.getContext('2d'))

  // ── canvas sizing ──────────────────────────────────────────
  function sizeCanvas(canvas, rows, columns, width, height) {
    canvas.width        = rows
    canvas.height       = columns
    canvas.style.width  = width  + 'px'
    canvas.style.height = height + 'px'
  }

  function updateSpriteCanvasSize(context) {
    const { cellwidth, cellheight, scale } = settings
    canvasContainer.style.width  = cellwidth  * scale + 32 + 'px'
    canvasContainer.style.height = cellheight * scale + 32 + 'px'
    sizeCanvas(canvasOverlay, cellwidth * scale, cellheight * scale, cellwidth * scale, cellheight * scale)
    const tmp = context.getImageData(0, 0, spriteCanvas.width, spriteCanvas.height)
    sizeCanvas(spriteCanvas, cellwidth, cellheight, cellwidth * scale, cellheight * scale)
    sizeCanvas(document.querySelector('#sprite-canvas-tmp'), cellwidth, cellheight, cellwidth * scale, cellheight * scale)
    context.putImageData(tmp, 0, 0)
  }

  function updateTilemapSize() {
    const { mapwidth, mapheight, mapscale } = settings
    const context = tilemap.getContext('2d')
    const tmp = context.getImageData(0, 0, tilemap.width, tilemap.height)
    sizeCanvas(tilemap,  mapwidth, mapheight, mapwidth  * mapscale, mapheight * mapscale)
    sizeCanvas(tilemapo, mapwidth * mapscale, mapheight * mapscale, mapwidth * mapscale, mapheight * mapscale)
    context.putImageData(tmp, 0, 0)
  }

  function updateWorldMapSize() {
    const { worldwidth, worldheight, worldscale } = settings
    const wc  = document.querySelector('#world-canvas')
    const wco = document.querySelector('#world-canvas-overlay')
    const wct = document.querySelector('#world-canvas-tmp')
    if (!wc || !wco || !wct) return
    sizeCanvas(wc,  worldwidth, worldheight, worldwidth * worldscale, worldheight * worldscale)
    sizeCanvas(wct, worldwidth, worldheight, worldwidth * worldscale, worldheight * worldscale)
    sizeCanvas(wco, worldwidth * worldscale, worldheight * worldscale, worldwidth * worldscale, worldheight * worldscale)
  }

  // ── tilemap navigation ─────────────────────────────────────
  function getTilemapPos(e) {
    const { cellwidth, cellheight, mapwidth, mapheight, mapscale } = settings
    const x = Math.floor(e.offsetX / mapscale) || 0
    const y = Math.floor(e.offsetY / mapscale) || 0
    if (x < 0 || x > mapwidth || y < 0 || y > mapheight) return { x: -1, y: -1 }
    return {
      x: Math.min(Math.floor(x / cellwidth),  (mapwidth  / cellwidth)  - 1),
      y: Math.min(Math.floor(y / cellheight), (mapheight / cellheight) - 1),
    }
  }

  function renderSelectedTileMapCell(ctx) {
    const { cellwidth, cellheight, mapwidth, mapheight, mapscale } = settings
    const [tx, ty] = selectedTilemapIndices
    ctx.imageSmoothingEnabled = false
    ctx.lineWidth = 2
    ctx.strokeStyle = 'rgba(0,0,0,1)'
    ctx.strokeRect(-2 + tx * cellwidth * mapscale, -2 + ty * cellheight * mapscale,
      4 + cellwidth * mapscale, 4 + cellheight * mapscale)
    ctx.strokeStyle = 'rgba(255,255,255,1)'
    ctx.strokeRect(tx * cellwidth * mapscale, ty * cellheight * mapscale, cellwidth * mapscale, cellheight * mapscale)
  }

  function mouseoutTilemap(e, context) {
    const { mapwidth, mapheight, mapscale } = settings
    context.clearRect(0, 0, mapwidth * mapscale, mapheight * mapscale)
    renderSelectedTileMapCell(context)
  }

  function mousemoveTilemap(e, ctx) {
    const { cellwidth, cellheight, mapwidth, mapheight, mapscale } = settings
    const { x, y } = getTilemapPos(e)
    ctx.clearRect(0, 0, mapwidth * mapscale, mapheight * mapscale)
    renderSelectedTileMapCell(ctx)
    ctx.strokeStyle = 'rgba(200,200,200,0.6)'
    ctx.lineWidth = 1
    ctx.setLineDash([2])
    ctx.strokeRect(x * cellwidth * mapscale, y * cellheight * mapscale, cellwidth * mapscale, cellheight * mapscale)
    ctx.setLineDash([])
  }

  function mousedownTilemap(e, ctx) {
    const { x, y } = getTilemapPos(e)
    selectedTilemapIndices = [x, y]
    copyTilemapToSprite()
    mousemoveTilemap(e, ctx)
    updateSpriteStatus()
  }

  function copySpriteToTilemap() {
    const { cellwidth, cellheight } = settings
    const tmp = spriteCanvas.getContext('2d').getImageData(0, 0, cellwidth, cellheight)
    const [tx, ty] = selectedTilemapIndices
    tilemap.getContext('2d').putImageData(tmp, tx * cellwidth, ty * cellheight)
  }

  function copyTilemapToSprite() {
    const { cellwidth, cellheight } = settings
    const tmp = spriteCanvas.getContext('2d')
    tmp.clearRect(0, 0, cellwidth, cellheight)
    const [tx, ty] = selectedTilemapIndices
    const tile = makeCanvasFromContextSub(tilemap.getContext('2d'), tx * cellwidth, ty * cellheight, cellwidth, cellheight)
    tmp.drawImage(tile, 0, 0, cellwidth, cellheight)
  }

  // ── undo/redo ──────────────────────────────────────────────
  function addToUndoStack() {
    const frame = frames[framePointer]
    const { undoStack, undoPointer } = frame
    if (undoPointer + 1 < undoStack.length) undoStack.splice(undoPointer + 1, undoStack.length - (undoPointer + 1))
    copySpriteToTilemap()
    const current = tilemap.toDataURL()
    if (undoStack.length === 0 || undoStack[undoStack.length - 1] !== current) {
      undoStack.push(current)
      frame.undoPointer++
    }
    updateHistoryButtons()
  }

  function undo(context) {
    const frame = frames[framePointer]
    if (frame.undoPointer > 0) { frame.undoPointer--; restoreFromUndoStack() }
    updateHistoryButtons()
  }

  function redo(context) {
    const frame = frames[framePointer]
    if (frame.undoPointer < frame.undoStack.length - 1) { frame.undoPointer++; restoreFromUndoStack() }
    updateHistoryButtons()
  }

  function updateHistoryButtons() {
    const frame = frames[framePointer]
    if (!frame || !undoButton || !redoButton) return
    undoButton.disabled = frame.undoPointer <= 0
    redoButton.disabled = frame.undoPointer >= frame.undoStack.length - 1
  }

  function restoreFromUndoStack() {
    const { mapwidth, mapheight } = settings
    const frame = frames[framePointer]
    const img = new Image()
    img.onload = function () {
      const ctx = tilemap.getContext('2d')
      ctx.clearRect(0, 0, img.naturalWidth, img.naturalHeight)
      ctx.drawImage(img, 0, 0, img.naturalWidth, img.naturalHeight)
      copyTilemapToSprite()
    }
    img.src = frame.undoStack[frame.undoPointer]
  }

  // ── frames ─────────────────────────────────────────────────
  function addFrame(empty = true) {
    const frame = empty ? emptyCanvas : tilemap.toDataURL()
    frames.splice(framePointer + 1, 0, { data: frame, undoStack: [frame], undoPointer: 0 })
    makeFramesUI()
  }

  function removeFrame() {
    if (frames.length <= 1) return
    frames.splice(framePointer, 1)
    if (framePointer >= frames.length) framePointer = frames.length - 1
    gotoFrameAtIndex(framePointer, false)
    makeFramesUI()
  }

  function gotoFrameAtIndex(index, allowSavingBefore = true) {
    frameIsReady = false
    if (framePointer >= 0 && allowSavingBefore) frames[framePointer].data = tilemap.toDataURL()
    framePointer = ((index % frames.length) + frames.length) % frames.length
    const img = new Image()
    img.onload = function () {
      const ctx = tilemap.getContext('2d')
      ctx.clearRect(0, 0, img.naturalWidth, img.naturalHeight)
      ctx.drawImage(img, 0, 0)
      frameIsReady = true
      if (mousePressed) mousedownCanvas(mousePressedEvent)
      copyTilemapToSprite()
      updateHistoryButtons()
    }
    img.src = frames[framePointer].data
    setCorrectFrameActiveClass()
  }

  function makeFramesUI() {
    framesList.innerHTML = ''
    frames.forEach((_, i) => {
      const d = document.createElement('div')
      framesList.appendChild(d)
      d.addEventListener('mousedown', () => gotoFrameAtIndex(i))
    })
    setCorrectFrameActiveClass()
  }

  function setCorrectFrameActiveClass() {
    Array.from(framesList.children).forEach((el, i) => {
      el.className = i === framePointer ? 'active' : ''
      el.innerText = i === framePointer ? i : ''
    })
  }

  // ── drawing ────────────────────────────────────────────────
  function setImageData(id, rgba) {
    for (let i = 0; i < id.data.length; i += 4) {
      id.data[i]     = rgba[0]
      id.data[i + 1] = rgba[1]
      id.data[i + 2] = rgba[2]
      id.data[i + 3] = 255
    }
  }

  function createEraserImageData(id) {
    for (let i = 0; i < id.data.length; i += 4) id.data[i + 3] = 0
  }

  function isContextBlank(context) {
    return !context.getImageData(0, 0, context.canvas.width, context.canvas.height).data.some(c => c !== 0)
  }

  function getContextToDrawIn() {
    const temp = document.querySelector('#sprite-canvas-tmp').getContext('2d')
    return isContextBlank(temp) ? spriteCanvas.getContext('2d') : temp
  }

  function getPixelPos(e) {
    return {
      x: Math.floor(e.offsetX / settings.scale) || 0,
      y: Math.floor(e.offsetY / settings.scale) || 0,
    }
  }

  function paintOnCanvas(context, x, y, data) {
    const { brushsize, rgba } = tooldata
    for (let i = -(brushsize - 1) / 2; i <= (brushsize - 1) / 2; i++) {
      for (let j = -(brushsize - 1) / 2; j <= (brushsize - 1) / 2; j++) {
        let draw = true
        if (selection) {
          const { tl, br } = getRect()
          if (!pointInRect(Math.floor(x + i), Math.floor(y + j), tl.x, tl.y, br.x, br.y)) draw = false
        }
        if (draw && Math.hypot(i, j) <= Math.floor(brushsize / 2) + 0.5) {
          if (data.data[3] === 255) {
            context.fillStyle = `rgba(${rgba[0]},${rgba[1]},${rgba[2]},1)`
            context.fillRect(Math.floor(x + i), Math.floor(y + j), 1, 1)
          } else {
            context.clearRect(Math.floor(x + i), Math.floor(y + j), 1, 1)
          }
        }
      }
    }
  }

  function mousedownCanvas(e) {
    mousePressed = true
    const { action } = tooldata
    const { x, y } = getPixelPos(e)

    // shift+click = eyedropper (independent of which tool is active)
    if (e.shiftKey && e.which === 1) {
      const px = x * settings.scale + (settings.scale >> 1)   // sample cell center
      const py = y * settings.scale + (settings.scale >> 1)
      const [r, g, b, a] = getColorAt(ctx, px, py)
      if (a > 0) {
        tooldata.rgba = [r, g, b, 255]
        setImageData(tooldata.id, tooldata.rgba)
        // sync the swatch palette: mark the matching swatch active
        const swatches = document.querySelectorAll('.swatches div')
        swatches.forEach(s => {
          const m = s.style.backgroundColor.match(/\d+/g)
          if (m && +m[0] === r && +m[1] === g && +m[2] === b) {
            swatches.forEach(o => o.className = '')
            s.className = 'active'
          }
        })
      }
      mousePressed = false   // don't drag-paint after picking
      return
    }

    if (action === 'fill') { floodfill(x, y, e.which); addToUndoStack(); return }
    if (e.which === 1) {
      if (action === 'selection') {
        selection = selection ? { x1: x, x2: x, y1: y, y2: y } : { x1: x, x2: x, y1: y, y2: y }
        renderSelectionRectangle()
      }
      if (action === 'linetool') lineToolData = { x1: x, x2: x, y1: y, y2: y }
    }
    if (action === 'stamp') { pasteClipboard(e, ctx); addToUndoStack() }
    mousemoveCanvas(e)
  }

  function mouseupCanvas(e) {
    const { action } = tooldata
    if (action === 'selection') {
      const { x, y } = getPixelPos(e)
      if (x === selection?.x1 && y === selection?.y1) {
        selection = undefined
        const { cellwidth, cellheight, scale } = settings
        canvasOverlay.getContext('2d').clearRect(0, 0, cellwidth * scale, cellheight * scale)
        pasteTempSelectionToMain()
      } else if (selection) {
        // real drag — auto-copy to the clipboard so the stamp tool just works
        // (no need for Cmd+C anymore)
        copyToClipboard(ctx)
      }
    }
    if (e.which === 1 && action === 'linetool') {
      const { cellwidth, cellheight } = settings
      document.querySelector('#sprite-canvas-tmp').getContext('2d').clearRect(0, 0, cellwidth, cellheight)
      const { x1, y1, x2, y2 } = lineToolData
      getLinePoints(x1, y1, x2, y2).forEach(([px, py]) => paintOnCanvas(getContextToDrawIn(), px, py, tooldata.id))
      addToUndoStack()
    }
  }

  function getSpriteIndex() {
    const { cellwidth, mapwidth } = settings
    const cols = Math.floor(mapwidth / cellwidth)
    const [tx, ty] = selectedTilemapIndices
    return ty * cols + tx
  }

  function updateSpriteStatus(pixelX, pixelY) {
    const statusEl = document.querySelector('.sprite-status')
    if (!statusEl) return
    const idx = getSpriteIndex()
    const tool = tooldata.action === 'linetool' ? lineToolFunctions[lineToolIndex] : tooldata.action
    if (pixelX !== undefined) {
      statusEl.textContent = `(${pixelX}, ${pixelY})   sprite #${idx}   ${tool}`
    } else {
      statusEl.textContent = `sprite #${idx}   ${tool}`
    }
  }

  function mouseoutCanvas() {
    if (!selection) {
      const { cellwidth, cellheight, scale } = settings
      canvasOverlay.getContext('2d').clearRect(0, 0, cellwidth * scale, cellheight * scale)
    }
    updateSpriteStatus()
  }

  function mouseup() {
    mousePressed = false
    if (dirtyFlag) { dirtyFlag = false; addToUndoStack() }
  }

  function mousemoveCanvas(e) {
    mousePressedEvent = { which: e.which, offsetX: e.offsetX, offsetY: e.offsetY }
    const { x, y } = getPixelPos(e)
    const { cellwidth, cellheight } = settings
    if (x >= 0 && y >= 0 && x < cellwidth && y < cellheight) updateSpriteStatus(x, y)
    if (!frameIsReady) return
    const { action } = tooldata
    if (mousePressed && action === 'pixel') {
      const { x, y } = getPixelPos(e)
      paintOnCanvas(getContextToDrawIn(), x, y, e.which === 1 ? tooldata.id : tooldata.eraser)
      dirtyFlag = true
    }
    if (e.which === 1 && mousePressed && action === 'selection') {
      const { x, y } = getPixelPos(e)
      selection.x2 = x; selection.y2 = y
      renderSelectionRectangle()
    }
    if (e.which === 1 && mousePressed && action === 'linetool') {
      const { x, y } = getPixelPos(e)
      lineToolData.x2 = x; lineToolData.y2 = y
      renderTemporaryLineToolPixels()
    }
    if (action === 'stamp') renderStampSelectionRectangle(e)
  }

  function mousedownSwatches(e) {
    Array.from(e.target.parentNode.children).forEach(s => s.className = '')
    e.target.className = 'active'
    const arr = e.target.style.backgroundColor.replace('rgb(', '').replace(')', '').split(',')
    tooldata.rgba = arr.map(c => parseInt(c, 10))   // numbers, not strings — strict-equals checks rely on this
    setImageData(tooldata.id, tooldata.rgba)
  }

  // ── selection ──────────────────────────────────────────────
  function getRect() {
    const { cellwidth, cellheight } = settings
    let tl = { x: 0, y: 0 }, br = { x: cellwidth, y: cellheight }
    if (selection) {
      const { x1, x2, y1, y2 } = selection
      tl = { x: Math.min(x1, x2), y: Math.min(y1, y2) }
      br = { x: Math.max(x1, x2), y: Math.max(y1, y2) }
    }
    const width = br.x - tl.x, height = br.y - tl.y
    br.x--; br.y--
    return { tl, br, width, height }
  }

  function renderSelectionRectangle() {
    const { cellwidth, cellheight, scale } = settings
    const ctx = canvasOverlay.getContext('2d')
    ctx.clearRect(0, 0, cellwidth * scale, cellheight * scale)
    ctx.imageSmoothingEnabled = false
    const { tl, br, width, height } = getRect()
    ctx.setLineDash([2]); ctx.lineWidth = 2
    ctx.strokeStyle = 'rgba(0,0,0,1)'
    ctx.strokeRect(tl.x * scale, tl.y * scale, width * scale, height * scale)
  }

  function renderStampSelectionRectangle(e) {
    const { cellwidth, cellheight, scale } = settings
    const ctx = canvasOverlay.getContext('2d')
    ctx.clearRect(0, 0, cellwidth * scale, cellheight * scale)
    if (clipboard) {
      const { x, y } = getPixelPos(e)
      ctx.setLineDash([]); ctx.strokeStyle = 'rgba(0,0,0,1)'
      ctx.strokeRect(x * scale, y * scale, clipboard.width * scale, clipboard.height * scale)
    }
  }

  function moveSelectionCanvas(x, y) {
    const { tl, br, width, height } = getRect()
    const vanillaCtx = spriteCanvas.getContext('2d')
    const tempCtx    = document.querySelector('#sprite-canvas-tmp').getContext('2d')
    const src = isContextBlank(tempCtx) ? vanillaCtx : tempCtx
    const tmp = src.getImageData(tl.x, tl.y, width, height)
    src.clearRect(tl.x, tl.y, width, height)
    tempCtx.putImageData(tmp, tl.x + x, tl.y + y)
    selection.x1 += x; selection.x2 += x; selection.y1 += y; selection.y2 += y
    renderSelectionRectangle()
  }

  function pasteTempSelectionToMain() {
    const { cellwidth, cellheight } = settings
    const tempCtx  = document.querySelector('#sprite-canvas-tmp').getContext('2d')
    const mainCtx  = spriteCanvas.getContext('2d')
    copyOpaquePixels(tempCtx, mainCtx, cellwidth, cellheight, 0, 0)
    tempCtx.clearRect(0, 0, cellwidth, cellheight)
  }

  function copyOpaquePixels(from, to, width, height, x, y) {
    const data = from.getImageData(0, 0, width, height).data
    for (let col = 0; col < width; col++) {
      for (let row = 0; row < height; row++) {
        const offset = ((row * width) + col) * 4
        if (data[3 + offset] === 255) {
          to.fillStyle = `rgba(${data[offset]},${data[offset+1]},${data[offset+2]},1)`
          to.fillRect(x + col, y + row, 1, 1)
        }
      }
    }
  }

  // ── transforms ─────────────────────────────────────────────
  function flipCanvasOrSelection(horizontal, vertical) {
    const context = getContextToDrawIn()
    const { tl, width, height } = getRect()
    if (width < 1 || height < 1) return
    const tmp = makeCanvasFromContextSub(context, tl.x, tl.y, width, height)
    context.clearRect(tl.x, tl.y, width, height)
    context.save()
    if (horizontal) { context.scale(-1, 1);  context.drawImage(tmp, 0, 0, width, height, -tl.x, tl.y, -width, height) }
    if (vertical)   { context.scale(1, -1);  context.drawImage(tmp, 0, 0, width, height, tl.x, -tl.y, width, -height) }
    context.restore()
    addToUndoStack()
  }

  function rotateCanvasOrSelection() {
    const context = getContextToDrawIn()
    const { tl, width, height } = getRect()
    if (width < 1 || height < 1 || width !== height) return
    const tmp = makeCanvasFromContextSub(context, tl.x, tl.y, width, height)
    context.clearRect(tl.x, tl.y, width, height)
    context.save()
    context.translate(width / 2, height / 2)
    context.rotate(Math.PI / 2)
    context.translate(-width / 2, -height / 2)
    context.drawImage(tmp, tl.y, -tl.x)
    context.restore()
    addToUndoStack()
  }

  function wrapAroundCanvas(x, y, context) {
    const { cellwidth, cellheight } = settings
    const tmp = context.getImageData(0, 0, cellwidth, cellheight)
    context.putImageData(tmp, x, y)
    if (x < 0) context.putImageData(tmp, cellwidth  + x, y)
    if (x > 0) context.putImageData(tmp, -cellwidth + x, y)
    if (y < 0) context.putImageData(tmp, x, cellheight  + y)
    if (y > 0) context.putImageData(tmp, x, -cellheight + y)
    addToUndoStack()
  }

  // ── clipboard ──────────────────────────────────────────────
  function copyToClipboard(context) {
    const { tl, width, height } = getRect()
    if (width < 1 || height < 1) return
    clipboard = makeCanvasFromContextSub(context, tl.x, tl.y, width, height)
  }

  function pasteClipboard(e, to) {
    if (!clipboard) return
    const { x, y } = getPixelPos(e)
    copyOpaquePixels(clipboard.getContext('2d'), to, clipboard.width, clipboard.height, x, y)
  }

  // ── line/shape tools ───────────────────────────────────────
  function getLinePoints(x0, y0, x1, y1) {
    const fn = lineToolFunctions[lineToolIndex]
    if (fn === 'line')      return line(x0, y0, x1, y1)
    if (fn === 'circle')    return ellipse(x0, y0, x1, y1)
    if (fn === 'rectangle') return rectangle(x0, y0, x1, y1)
    return []
  }

  function renderTemporaryLineToolPixels() {
    const { cellwidth, cellheight } = settings
    const tmpCtx = document.querySelector('#sprite-canvas-tmp').getContext('2d')
    tmpCtx.clearRect(0, 0, cellwidth, cellheight)
    const { x1, y1, x2, y2 } = lineToolData
    getLinePoints(x1, y1, x2, y2).forEach(([px, py]) => paintOnCanvas(tmpCtx, px, py, tooldata.id))
  }

  function line(x0, y0, x1, y1) {
    const dx = Math.abs(x1 - x0), dy = Math.abs(y1 - y0)
    const sx = x0 < x1 ? 1 : -1,  sy = y0 < y1 ? 1 : -1
    let err = dx - dy
    const result = []
    while (true) {
      result.push([x0, y0])
      if (x0 === x1 && y0 === y1) break
      const e2 = 2 * err
      if (e2 > -dy) { err -= dy; x0 += sx }
      if (e2 < dx)  { err += dx; y0 += sy }
    }
    return result
  }

  function ellipse(x0, y0, x1, y1) {
    const cx = x0 + (x1 - x0) / 2, cy = y0 + (y1 - y0) / 2
    const rx = (x1 - x0) / 2,      ry = (y1 - y0) / 2
    const result = []
    let lx = -1, ly = -1
    for (let a = 0; a <= 720; a++) {
      const px = parseInt(cx + rx * Math.cos(a * 2 * Math.PI / 720) + 0.5)
      const py = parseInt(cy + ry * Math.sin(a * 2 * Math.PI / 720) + 0.5)
      if (px !== lx || py !== ly) { result.push([px, py]); lx = px; ly = py }
    }
    return result
  }

  function rectangle(x0, y0, x1, y1) {
    return [...line(x0, y0, x1, y0), ...line(x1, y0, x1, y1), ...line(x0, y1, x1, y1), ...line(x0, y0, x0, y1)]
  }

  // ── flood fill ─────────────────────────────────────────────
  function floodfill(x, y, which) {
    const { tl, br } = getRect()
    const context = getContextToDrawIn()
    // the exact color this fill will paint (alpha 0 when erasing)
    const paint = which === 1
      ? [+tooldata.rgba[0], +tooldata.rgba[1], +tooldata.rgba[2], 255]
      : [0, 0, 0, 0]
    const [r1, g1, b1, a1] = getColorAt(context, x, y)
    // already the target color? painting wouldn't change anything — bail, or we'd loop forever
    if (r1 === paint[0] && g1 === paint[1] && b1 === paint[2] && a1 === paint[3]) return
    const stack = pointInRect(x, y, tl.x, tl.y, br.x, br.y) ? [[x, y]] : []
    while (stack.length > 0) {
      const [cx, cy] = stack.pop()
      const [r2, g2, b2, a2] = getColorAt(context, cx, cy)
      if (r1 === r2 && g1 === g2 && b1 === b2 && a1 === a2) {
        context.putImageData(which === 1 ? tooldata.id : tooldata.eraser, cx, cy)
        if (cy < br.y) stack.push([cx, cy + 1])
        if (cy > tl.y) stack.push([cx, cy - 1])
        if (cx < br.x) stack.push([cx + 1, cy])
        if (cx > tl.x) stack.push([cx - 1, cy])
      }
    }
  }

  // ── helpers ────────────────────────────────────────────────
  function getColorAt(context, x, y) { return context.getImageData(x, y, 1, 1).data }
  function pointInRect(px, py, x1, y1, x2, y2) { return px >= x1 && px <= x2 && py >= y1 && py <= y2 }

  function makeCanvasFromContextSub(context, x, y, width, height) {
    const tmp = context.getImageData(x, y, width, height)
    const c   = document.createElement('canvas')
    c.width = width; c.height = height
    c.getContext('2d').putImageData(tmp, 0, 0)
    return c
  }

  // ── keyboard ───────────────────────────────────────────────
  function keydownHandler(e) {
    if (e.target.nodeName === 'INPUT') return
    // only handle sprite-editor shortcuts when the sprites tab is showing,
    // otherwise Cmd+Z etc. would fire while editing code on the code tab
    if (!document.getElementById('panel-sprites')?.classList.contains('active')) return
    if (selection) {
      if (e.key === 'ArrowUp')    moveSelectionCanvas(0, -1)
      if (e.key === 'ArrowDown')  moveSelectionCanvas(0,  1)
      if (e.key === 'ArrowLeft')  moveSelectionCanvas(-1, 0)
      if (e.key === 'ArrowRight') moveSelectionCanvas(1,  0)
    } else {
      if (e.key === 'ArrowUp')    wrapAroundCanvas(0, -1, ctx)
      if (e.key === 'ArrowDown')  wrapAroundCanvas(0,  1, ctx)
      if (e.key === 'ArrowLeft')  wrapAroundCanvas(-1, 0, ctx)
      if (e.key === 'ArrowRight') wrapAroundCanvas(1,  0, ctx)
    }
    if (e.key === 'h' && !e.metaKey) flipCanvasOrSelection(true, false)
    if (e.key === 'v' && !e.metaKey) flipCanvasOrSelection(false, true)
    if (e.key === 'r' && !e.metaKey) rotateCanvasOrSelection()
    if (e.key === '1') { if (dirtyFlag) addToUndoStack(); gotoFrameAtIndex(framePointer - 1) }
    if (e.key === '2') { if (dirtyFlag) addToUndoStack(); gotoFrameAtIndex(framePointer + 1) }
    if (e.key === '3') { if (dirtyFlag) addToUndoStack(); addFrame(); gotoFrameAtIndex(framePointer + 1) }
    if (e.key === '4') { if (dirtyFlag) addToUndoStack(); addFrame(false); gotoFrameAtIndex(framePointer + 1) }
    if (e.metaKey && e.key === 'z') { e.preventDefault(); undo(ctx) }
    if (e.metaKey && e.key === 'y') { e.preventDefault(); redo(ctx) }
    if (e.shiftKey && e.metaKey && e.key === 'z') { e.preventDefault(); redo(ctx) }
    if (e.key === 'p') setToolActive('pixel')
    if (e.key === 'f') setToolActive('fill')
    if (e.key === 't') setToolActive('stamp')
    if (e.key === 's' && !e.metaKey) setToolActive('selection')
    if (e.key === 'l' && !e.metaKey) {
      if (tooldata.action === 'linetool') { lineToolIndex = (lineToolIndex + 1) % lineToolFunctions.length; makeLineToolUI() }
      setToolActive('linetool')
    }
    if (e.key === 'd') { removeFrame(); addToUndoStack() }
    if (e.key === '\\') { localStorage.setItem('frames', JSON.stringify(frames)) }
    if (e.metaKey && e.key === 'c') { e.preventDefault(); copyToClipboard(ctx) }
    if (e.metaKey && e.key === 'v') { e.preventDefault(); pasteClipboard(e, ctx); addToUndoStack() }
  }

  // ── tools ui ───────────────────────────────────────────────
  function setToolActive(id) {
    TOOL_IDS.forEach(tid => {
      const el = document.getElementById(tid)
      if (!el) return
      el.className = tid === id ? 'active' : ''
      if (tid === id) tooldata.action = id
    })
    updateSpriteStatus()
  }

  function makeLineToolUI() {
    document.querySelector('#linetool').innerText = lineToolFunctions[lineToolIndex]
  }

  TOOL_IDS.forEach(id => {
    const c = document.getElementById(id)
    if (!c) return
    c.addEventListener('click', () => {
      if (id === 'linetool' && tooldata.action === 'linetool') {
        lineToolIndex = (lineToolIndex + 1) % lineToolFunctions.length
        makeLineToolUI()
      }
      setToolActive(id)
    })
  })

  // ── controls wiring ────────────────────────────────────────
  const cwResizer = document.querySelector('#cellwidth')
  cwResizer.value = settings.cellwidth
  cwResizer.addEventListener('change', e => { settings.cellwidth = Number(e.target.value); updateSpriteCanvasSize(ctx); addToUndoStack() })

  const chResizer = document.querySelector('#cellheight')
  chResizer.value = settings.cellheight
  chResizer.addEventListener('change', e => { settings.cellheight = Number(e.target.value); updateSpriteCanvasSize(ctx); addToUndoStack() })

  // zoom is locked at 100% (settings.scale = 28). slider was removed.

  const brushSlider = document.querySelector('#brushRange')
  const brushInput  = document.querySelector('#brushInput')
  brushSlider.value = tooldata.brushsize
  brushInput.value  = tooldata.brushsize
  brushSlider.addEventListener('input',  e => { tooldata.brushsize = e.target.value; brushInput.value  = e.target.value })
  brushInput.addEventListener('change', e => { tooldata.brushsize = e.target.value; brushSlider.value = e.target.value })

  undoButton.addEventListener('click', () => { undo(ctx) })
  redoButton.addEventListener('click', () => { redo(ctx) })
  updateHistoryButtons()
  updateSpriteStatus()

  // ── export ─────────────────────────────────────────────────
  // Both exporters snapshot the live sprite cell back into the sheet first,
  // so the file matches exactly what's on screen.
  function download(blob, name) {
    const url = URL.createObjectURL(blob)
    const a = document.createElement('a')
    a.href = url; a.download = name
    document.body.appendChild(a); a.click(); a.remove()
    URL.revokeObjectURL(url)
  }

  // PNG = the current frame's 128×128 sheet, 1:1 pixels.
  function exportPng() {
    copySpriteToTilemap()
    tilemap.toBlob(blob => download(blob, 'spritesheet.png'))
  }

  // Aseprite (.aseprite / .ase) binary writer. Little-endian throughout.
  // Spec: https://github.com/aseprite/aseprite/blob/main/docs/ase-file-specs.md
  function makeAseBuf() {
    const b = []
    return {
      b,
      u8:  v => b.push(v & 0xff),
      u16: v => { b.push(v & 0xff, (v >>> 8) & 0xff) },
      i16: function (v) { this.u16(v & 0xffff) },
      u32: v => { b.push(v & 0xff, (v >>> 8) & 0xff, (v >>> 16) & 0xff, (v >>> 24) & 0xff) },
      bytes: arr => { for (let i = 0; i < arr.length; i++) b.push(arr[i] & 0xff) },
      str: function (s) { const e = new TextEncoder().encode(s); this.u16(e.length); this.bytes(e) },
      pos: () => b.length,
      patchU32: (p, v) => { b[p] = v & 0xff; b[p + 1] = (v >>> 8) & 0xff; b[p + 2] = (v >>> 16) & 0xff; b[p + 3] = (v >>> 24) & 0xff },
    }
  }

  function aseChunk(buf, type, fn) {
    const start = buf.pos()
    buf.u32(0)          // chunk size (backpatched)
    buf.u16(type)
    fn()
    buf.patchU32(start, buf.pos() - start)
  }

  function aseFrame(buf, durationMs, chunkCount, fn) {
    const start = buf.pos()
    buf.u32(0)                                              // frame bytes (backpatched)
    buf.u16(0xF1FA)                                         // frame magic
    buf.u16(chunkCount > 0xffff ? 0xffff : chunkCount)      // old chunk count
    buf.u16(durationMs)
    buf.bytes([0, 0])                                       // reserved
    buf.u32(chunkCount)                                     // new chunk count
    fn()
    buf.patchU32(start, buf.pos() - start)
  }

  const loadImage = src => new Promise(res => { const im = new Image(); im.onload = () => res(im); im.src = src })

  // Render one frame's dataURL to native-size RGBA bytes (row-major, R,G,B,A).
  async function framePixels(dataURL, w, h) {
    const img = await loadImage(dataURL)
    const c = document.createElement('canvas'); c.width = w; c.height = h
    const cx = c.getContext('2d'); cx.imageSmoothingEnabled = false
    cx.clearRect(0, 0, w, h)
    cx.drawImage(img, 0, 0, w, h)
    return cx.getImageData(0, 0, w, h).data
  }

  async function exportAse() {
    await paletteReady
    const { mapwidth: w, mapheight: h } = settings
    // sync the live cell, then snapshot every frame's sheet
    copySpriteToTilemap()
    frames[framePointer].data = tilemap.toDataURL()
    const pix = []
    for (const f of frames) pix.push(await framePixels(f.data, w, h))

    const buf = makeAseBuf()
    // ── file header (128 bytes) ──
    buf.u32(0)              // file size (backpatched)
    buf.u16(0xA5E0)         // magic
    buf.u16(frames.length)  // frame count
    buf.u16(w); buf.u16(h)
    buf.u16(32)             // color depth: RGBA (lossless — keeps real transparency)
    buf.u32(1)              // flags: layer opacity valid
    buf.u16(100)            // speed (deprecated)
    buf.u32(0); buf.u32(0)
    buf.u8(0)               // transparent index (unused in RGBA)
    buf.bytes([0, 0, 0])
    buf.u16(palette.length) // number of colors
    buf.u8(1); buf.u8(1)    // pixel w/h ratio
    buf.i16(0); buf.i16(0)  // grid x/y
    buf.u16(16); buf.u16(16)// grid w/h (matches the 16×16 sprite cells)
    for (let i = 0; i < 84; i++) buf.u8(0)  // reserved → header is exactly 128 bytes

    for (let fi = 0; fi < frames.length; fi++) {
      // frame 0 also carries the palette + layer; every frame carries its cel
      const chunkCount = fi === 0 ? 4 : 1
      aseFrame(buf, 100, chunkCount, () => {
        if (fi === 0) {
          // new palette chunk (0x2019) — what modern Aseprite reads
          aseChunk(buf, 0x2019, () => {
            buf.u32(palette.length); buf.u32(0); buf.u32(palette.length - 1)
            buf.bytes([0, 0, 0, 0, 0, 0, 0, 0])
            for (const [r, g, b] of palette) { buf.u16(0); buf.u8(r); buf.u8(g); buf.u8(b); buf.u8(255) }
          })
          // old palette chunk (0x0004) — for older readers' swatch panel
          aseChunk(buf, 0x0004, () => {
            buf.u16(1); buf.u8(0); buf.u8(palette.length === 256 ? 0 : palette.length)
            for (const [r, g, b] of palette) { buf.u8(r); buf.u8(g); buf.u8(b) }
          })
          // layer chunk (0x2004)
          aseChunk(buf, 0x2004, () => {
            buf.u16(3)   // flags: visible | editable
            buf.u16(0)   // type: normal
            buf.u16(0)   // child level
            buf.u16(0); buf.u16(0)   // default w/h (ignored)
            buf.u16(0)   // blend: normal
            buf.u8(255)  // opacity
            buf.bytes([0, 0, 0])
            buf.str('sprites')
          })
        }
        // cel chunk (0x2005), raw RGBA image data
        aseChunk(buf, 0x2005, () => {
          buf.u16(0)              // layer index
          buf.i16(0); buf.i16(0)  // x/y position
          buf.u8(255)             // opacity
          buf.u16(0)              // cel type: 0 = raw image data
          buf.i16(0)              // z-index
          buf.bytes([0, 0, 0, 0, 0])   // reserved (z-index + 5 = 7 bytes after cel type)
          buf.u16(w); buf.u16(h)
          buf.bytes(pix[fi])      // w*h*4 RGBA bytes
        })
      })
    }

    buf.patchU32(0, buf.pos())   // file size
    download(new Blob([new Uint8Array(buf.b)], { type: 'application/octet-stream' }), 'spritesheet.aseprite')
  }

  document.querySelector('#export-png').addEventListener('click', e => { exportPng(); e.currentTarget.blur() })
  document.querySelector('#export-ase').addEventListener('click', e => { exportAse(); e.currentTarget.blur() })

  document.querySelector('#help-button').addEventListener('click', () => {
    const panel = document.querySelector('#help-text')
    panel.style.display = panel.style.display === 'block' ? 'none' : 'block'
  })

  window.addEventListener('de:load-sprites', e => {
    const img = new Image()
    img.onload = function () {
      const ctx = tilemap.getContext('2d')
      ctx.clearRect(0, 0, tilemap.width, tilemap.height)
      ctx.drawImage(img, 0, 0, img.naturalWidth, img.naturalHeight)
      frames[0].data = tilemap.toDataURL()
      frames[0].undoStack = [frames[0].data]
      frames[0].undoPointer = 0
      frames.length = 1
      framePointer = 0
      makeFramesUI()
      copyTilemapToSprite()
      updateHistoryButtons()
    }
    img.src = e.detail
  })
})
