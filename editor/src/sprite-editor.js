import './sprite-editor.css'

document.addEventListener('DOMContentLoaded', function () {

  // ── fetch palette ──────────────────────────────────────────
  fetch('/palettes/pico32.json')
    .then(r => r.json())
    .then(res => {
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

  // ── settings ───────────────────────────────────────────────
  const settings = {
    cellwidth:   16,
    cellheight:  16,
    scale:       16,
    mapwidth:    256,   // 16 cols × 16px
    mapheight:   256,   // 16 rows × 16px  → 256 sprites total
    mapscale:    2,
    worldwidth:  16,
    worldheight: 16,
    worldscale:  16,
  }

  let selectedTilemapIndices = [0, 0]

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
  }

  function undo(context) {
    const frame = frames[framePointer]
    if (frame.undoPointer > 0) { frame.undoPointer--; restoreFromUndoStack() }
  }

  function redo(context) {
    const frame = frames[framePointer]
    if (frame.undoPointer < frame.undoStack.length - 1) { frame.undoPointer++; restoreFromUndoStack() }
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
    frames.splice(framePointer + 1, 0, { data: frame, undoStack: [frame], undoPointer: 1 })
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

  function mouseoutCanvas() {
    if (!selection) {
      const { cellwidth, cellheight, scale } = settings
      canvasOverlay.getContext('2d').clearRect(0, 0, cellwidth * scale, cellheight * scale)
    }
  }

  function mouseup() {
    mousePressed = false
    if (dirtyFlag) { dirtyFlag = false; addToUndoStack() }
  }

  function mousemoveCanvas(e) {
    mousePressedEvent = { which: e.which, offsetX: e.offsetX, offsetY: e.offsetY }
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
    tooldata.rgba = arr
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
    let rgba = which !== 1 ? [0, 0, 0, 0] : tooldata.rgba
    const [r1, g1, b1, a1] = getColorAt(context, x, y)
    if (r1 === rgba[0] && g1 === rgba[1] && b1 === rgba[2]) return
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
    Array.from(togglegroup.children).forEach(el => {
      el.className = el.id === id ? 'active' : ''
      if (el.id === id) tooldata.action = id
    })
  }

  function makeLineToolUI() {
    document.querySelector('#linetool').innerText = lineToolFunctions[lineToolIndex]
  }

  Array.from(togglegroup.children).forEach(c => {
    c.addEventListener('click', () => {
      if (c.id === 'linetool' && tooldata.action === 'linetool') {
        lineToolIndex = (lineToolIndex + 1) % lineToolFunctions.length
        makeLineToolUI()
      }
      setToolActive(c.id)
    })
  })

  // ── controls wiring ────────────────────────────────────────
  const cwResizer = document.querySelector('#cellwidth')
  cwResizer.value = settings.cellwidth
  cwResizer.addEventListener('change', e => { settings.cellwidth = Number(e.target.value); updateSpriteCanvasSize(ctx); addToUndoStack() })

  const chResizer = document.querySelector('#cellheight')
  chResizer.value = settings.cellheight
  chResizer.addEventListener('change', e => { settings.cellheight = Number(e.target.value); updateSpriteCanvasSize(ctx); addToUndoStack() })

  const zoomSlider = document.querySelector('#zoomRange')
  zoomSlider.addEventListener('input', e => { settings.scale = Math.pow(2, 1 + parseInt(e.target.value)); updateSpriteCanvasSize(ctx) })

  const zoomTilemapSlider = document.querySelector('#zoomRangeTilemap')
  zoomTilemapSlider.addEventListener('input', e => { settings.mapscale = Number(e.target.value); updateTilemapSize() })

  const brushSlider = document.querySelector('#brushRange')
  const brushInput  = document.querySelector('#brushInput')
  brushSlider.value = tooldata.brushsize
  brushInput.value  = tooldata.brushsize
  brushSlider.addEventListener('input',  e => { tooldata.brushsize = e.target.value; brushInput.value  = e.target.value })
  brushInput.addEventListener('change', e => { tooldata.brushsize = e.target.value; brushSlider.value = e.target.value })

  document.querySelector('#help-button').addEventListener('click', () => {
    const panel = document.querySelector('#help-text')
    panel.style.display = panel.style.display === 'block' ? 'none' : 'block'
  })
})
