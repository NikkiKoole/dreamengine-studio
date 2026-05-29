const { app, BrowserWindow, ipcMain, Menu, session, dialog, shell } = require('electron')
const { exec, execSync, spawn }        = require('child_process')
const path                             = require('path')
const fs                               = require('fs')
const zlib                             = require('zlib')
const http                             = require('http')

// ── PNG cart helpers ──────────────────────────────────────────
function crc32(buf) {
  let crc = 0xFFFFFFFF
  for (let i = 0; i < buf.length; i++) {
    crc ^= buf[i]
    for (let j = 0; j < 8; j++) crc = (crc & 1) ? (0xEDB88320 ^ (crc >>> 1)) : (crc >>> 1)
  }
  return (crc ^ 0xFFFFFFFF) >>> 0
}

function makeChunk(type, data) {
  const typeBuf = Buffer.from(type, 'ascii')
  const lenBuf  = Buffer.allocUnsafe(4)
  lenBuf.writeUInt32BE(data.length)
  const crcBuf  = Buffer.allocUnsafe(4)
  crcBuf.writeUInt32BE(crc32(Buffer.concat([typeBuf, data])))
  return Buffer.concat([lenBuf, typeBuf, data, crcBuf])
}

function makeZtxtChunk(keyword, text) {
  const compressed = zlib.deflateSync(Buffer.from(text, 'utf8'))
  return makeChunk('zTXt', Buffer.concat([
    Buffer.from(keyword, 'latin1'),
    Buffer.from([0, 0]),   // null terminator + compression method 0
    compressed,
  ]))
}

function embedCartChunks(pngBuf, data) {
  const PNG_SIG = Buffer.from([137, 80, 78, 71, 13, 10, 26, 10])
  const parts = [PNG_SIG]
  let offset = 8
  let iend = null
  while (offset + 12 <= pngBuf.length) {
    const len  = pngBuf.readUInt32BE(offset)
    const type = pngBuf.slice(offset + 4, offset + 8).toString('ascii')
    const chunk = pngBuf.slice(offset, offset + 12 + len)
    if (type === 'IEND') { iend = chunk; break }
    parts.push(chunk)
    offset += 12 + len
  }
  for (const [key, val] of Object.entries(data)) parts.push(makeZtxtChunk(`de:${key}`, val))
  if (iend) parts.push(iend)
  return Buffer.concat(parts)
}

function extractCartChunks(pngBuf) {
  const result = {}
  let offset = 8
  while (offset + 12 <= pngBuf.length) {
    const len  = pngBuf.readUInt32BE(offset)
    const type = pngBuf.slice(offset + 4, offset + 8).toString('ascii')
    const data = pngBuf.slice(offset + 8, offset + 8 + len)
    if (type === 'zTXt') {
      const nullIdx = data.indexOf(0)
      if (nullIdx !== -1) {
        const key = data.slice(0, nullIdx).toString('latin1')
        if (key.startsWith('de:')) {
          try { result[key.slice(3)] = zlib.inflateSync(data.slice(nullIdx + 2)).toString('utf8') } catch {}
        }
      }
    }
    offset += 12 + len
    if (type === 'IEND') break
  }
  return result
}

const RUNTIME_DIR = path.join(__dirname, '../../runtime')
const BUILD_DIR   = path.join(__dirname, '../../build')
const RAYLIB      = fs.existsSync('/opt/homebrew/opt/raylib') ? '/opt/homebrew/opt/raylib' : '/usr/local/opt/raylib'
const RAYLIB_WIN  = fs.existsSync('/opt/homebrew/opt/raylib-win64/raylib-5.5_win64_mingw-w64')
  ? '/opt/homebrew/opt/raylib-win64/raylib-5.5_win64_mingw-w64'
  : '/usr/local/opt/raylib-win64/raylib-5.5_win64_mingw-w64'
const MINGW       = 'x86_64-w64-mingw32-gcc'
const CART_SRC    = path.join(BUILD_DIR, 'cart.c')
const CART_BIN    = path.join(BUILD_DIR, 'cart')
const CART_EXE    = path.join(BUILD_DIR, 'cart.exe')

function createWindow() {
  const win = new BrowserWindow({
    width: 1280,
    height: 800,
    titleBarStyle: 'hiddenInset',
    backgroundColor: '#1b1c1f',
    webPreferences: {
      preload: path.join(__dirname, 'preload.cjs'),
      contextIsolation: true,
    },
  })

  win.loadURL('http://localhost:5173')

  win.webContents.on('before-input-event', (event, input) => {
    if (!input.meta && !input.control) return
    switch (input.key.toLowerCase()) {
      case 'c': event.preventDefault(); win.webContents.copy();      break
      case 'v': event.preventDefault(); win.webContents.paste();     break
      case 'x': event.preventDefault(); win.webContents.cut();       break
      case 'a': event.preventDefault(); win.webContents.selectAll(); break
      // NOTE: Cmd/Ctrl+Z (and Shift/Y) are intentionally NOT intercepted here.
      // Letting them reach the renderer lets CodeMirror's own history handle
      // undo/redo on the code tab, and the sprite editor handle it on the
      // sprites tab. Intercepting natively here would block the sprite editor.
    }
  })
}

app.whenReady().then(() => {
  session.defaultSession.setPermissionRequestHandler((_wc, permission, callback) => {
    callback(permission === 'clipboard-read' || permission === 'clipboard-write' || permission === 'clipboard-sanitized-write')
  })
  session.defaultSession.setPermissionCheckHandler((_wc, permission) => {
    return permission === 'clipboard-read' || permission === 'clipboard-write' || permission === 'clipboard-sanitized-write'
  })
  Menu.setApplicationMenu(Menu.buildFromTemplate([
    {
      label: 'dreamengine',
      submenu: [{ role: 'quit' }]
    },
    {
      label: 'Edit',
      submenu: [
        // undo/redo are handled in the renderer (CodeMirror history on the code
        // tab, the sprite editor on the sprites tab). Registering the menu roles
        // here would claim the Cmd+Z accelerator and block them, so they're omitted.
        { role: 'cut' },
        { role: 'copy' },
        { role: 'paste' },
        { role: 'selectAll' },
      ]
    },
    {
      label: 'View',
      submenu: [
        { role: 'zoomIn' },
        { role: 'zoomOut' },
        { role: 'resetZoom' },
        { type: 'separator' },
        { role: 'togglefullscreen' },
        { role: 'toggleDevTools' },
      ]
    }
  ]))
  createWindow()
})

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') app.quit()
})


// ── cart save ─────────────────────────────────────────────────
ipcMain.handle('cart:save', async (_event, { source, spritesDataUrl, mapBase64 }) => {
  const { filePath } = await dialog.showSaveDialog({
    title: 'Save Cart',
    defaultPath: 'mycart.cart.png',
    filters: [{ name: 'Dreamengine Cart', extensions: ['png'] }],
  })
  if (!filePath) return { ok: false }

  const screenshotPng = path.join(BUILD_DIR, 'screenshot.png')
  const spritesPng    = path.join(BUILD_DIR, 'sprites.png')
  const basePng = fs.existsSync(screenshotPng)
    ? fs.readFileSync(screenshotPng)
    : fs.existsSync(spritesPng)
      ? fs.readFileSync(spritesPng)
      : Buffer.from(spritesDataUrl.replace(/^data:image\/png;base64,/, ''), 'base64')

  const cartPng = embedCartChunks(basePng, { source, sprites: spritesDataUrl, map: mapBase64 })
  fs.writeFileSync(filePath, cartPng)
  return { ok: true, filePath }
})

// ── cart load ─────────────────────────────────────────────────
ipcMain.handle('cart:load', async () => {
  const { filePaths } = await dialog.showOpenDialog({
    title: 'Load Cart',
    filters: [{ name: 'Dreamengine Cart', extensions: ['png'] }],
    properties: ['openFile'],
  })
  if (!filePaths || filePaths.length === 0) return null

  const chunks = extractCartChunks(fs.readFileSync(filePaths[0]))
  if (!chunks.source) return { ok: false, error: 'Not a dreamengine cart' }
  return { ok: true, source: chunks.source, spritesDataUrl: chunks.sprites || null, mapBase64: chunks.map || null }
})

ipcMain.handle('cart:load-buffer', async (_event, bytes) => {
  const chunks = extractCartChunks(Buffer.from(bytes))
  if (!chunks.source) return { ok: false, error: 'Not a dreamengine cart' }
  return { ok: true, source: chunks.source, spritesDataUrl: chunks.sprites || null, mapBase64: chunks.map || null }
})

ipcMain.handle('cart:load-file', async (_event, filePath) => {
  const chunks = extractCartChunks(fs.readFileSync(filePath))
  if (!chunks.source) return { ok: false, error: 'Not a dreamengine cart' }
  return { ok: true, source: chunks.source, spritesDataUrl: chunks.sprites || null, mapBase64: chunks.map || null }
})

// ── sprites handler ───────────────────────────────────────────
ipcMain.handle('studio:save-sprites', (_event, dataUrl) => {
  fs.mkdirSync(BUILD_DIR, { recursive: true })
  const base64 = dataUrl.replace(/^data:image\/png;base64,/, '')
  fs.writeFileSync(path.join(BUILD_DIR, 'sprites.png'), base64, 'base64')
})

// ── map handler ───────────────────────────────────────────────
// receives a Uint8Array (length MAP_W * MAP_H = 8192) of cell indices
ipcMain.handle('studio:save-map', (_event, bytes) => {
  fs.mkdirSync(BUILD_DIR, { recursive: true })
  fs.writeFileSync(path.join(BUILD_DIR, 'map.dat'), Buffer.from(bytes))
})

// ── run handler ───────────────────────────────────────────────
ipcMain.handle('studio:run', async (_event, code, cfg) => {
  const screenW       = cfg?.screenW || 320
  const screenH       = cfg?.screenH || 200
  const scale         = cfg?.scale   || 4
  const mapW          = cfg?.mapW    || 128
  const mapH          = cfg?.mapH    || 64
  const cellW         = cfg?.cellW   || 16
  const cellH         = cfg?.cellH   || 16
  const touchDefault  = cfg?.touchControls ? 1 : 0
  const studioC       = path.join(RUNTIME_DIR, 'studio.c')

  fs.mkdirSync(BUILD_DIR, { recursive: true })

  // remove any stale files from previous sessions
  const stale = ['ComicMono.ttf', 'ComicMono-Bold.ttf', 'PixelComicSans.ttf', 'Ac437_Acer_VGA_8x8.ttf']
  stale.forEach(f => { try { fs.unlinkSync(path.join(BUILD_DIR, f)) } catch {} })

  fs.writeFileSync(CART_SRC, code)

  // generate sprites_data.h from sprites.png via xxd (PNG is compressed — stays small)
  const spritesHeader = path.join(BUILD_DIR, 'sprites_data.h')
  const spritesPng    = path.join(BUILD_DIR, 'sprites.png')
  if (fs.existsSync(spritesPng)) {
    try {
      const xxd = execSync('xxd -i sprites.png', { cwd: BUILD_DIR }).toString()
      fs.writeFileSync(spritesHeader,
        xxd.replace(/unsigned char sprites_png\[\]/,  'static const unsigned char SPRITES_DATA[]')
           .replace(/unsigned int sprites_png_len/,   'static const unsigned int  SPRITES_DATA_LEN'))
    } catch {
      fs.writeFileSync(spritesHeader, 'static const unsigned char SPRITES_DATA[]={0};static const unsigned int SPRITES_DATA_LEN=0;\n')
    }
  } else {
    fs.writeFileSync(spritesHeader, 'static const unsigned char SPRITES_DATA[]={0};static const unsigned int SPRITES_DATA_LEN=0;\n')
  }

  // generate map_data.h from map.dat (raw 8192 bytes = MAP_W × MAP_H cell indices)
  const mapHeader = path.join(BUILD_DIR, 'map_data.h')
  const mapDat    = path.join(BUILD_DIR, 'map.dat')
  if (fs.existsSync(mapDat)) {
    try {
      const xxd = execSync('xxd -i map.dat', { cwd: BUILD_DIR }).toString()
      fs.writeFileSync(mapHeader,
        xxd.replace(/unsigned char map_dat\[\]/,  'static const unsigned char MAP_DATA[]')
           .replace(/unsigned int map_dat_len/,   'static const unsigned int  MAP_DATA_LEN'))
    } catch {
      fs.writeFileSync(mapHeader, 'static const unsigned char MAP_DATA[]={0};static const unsigned int MAP_DATA_LEN=0;\n')
    }
  } else {
    fs.writeFileSync(mapHeader, 'static const unsigned char MAP_DATA[]={0};static const unsigned int MAP_DATA_LEN=0;\n')
  }

  const args = [
    `"${CART_SRC}"`,
    `"${studioC}"`,
    `-I"${RUNTIME_DIR}"`,
    `-I"${BUILD_DIR}"`,
    `-I"${RAYLIB}/include"`,
    `-DSCREEN_W=${screenW}`,
    `-DSCREEN_H=${screenH}`,
    `-DSCALE=${scale}`,
    `-DMAP_W=${mapW}`,
    `-DMAP_H=${mapH}`,
    `-DCELL_W=${cellW}`,
    `-DCELL_H=${cellH}`,
    `-DTOUCH_CONTROLS_DEFAULT=${touchDefault}`,
    '-Os',
    // keep beginners' null-pointer mistakes as real crashes (caught by the
    // crash handler in studio.c) instead of letting the optimizer delete them
    '-fno-delete-null-pointer-checks',
    `"${RAYLIB}/lib/libraylib.a"`,
    '-framework OpenGL',
    '-framework Cocoa',
    '-framework IOKit',
    '-framework CoreVideo',
    '-framework CoreFoundation',
    '-Wl,-dead_strip',
    `-o "${CART_BIN}"`,
  ]

  const cmd = `clang ${args.join(' ')}`

  return new Promise(resolve => {
    exec(cmd, (err, _stdout, stderr) => {
      // filter out the noisy macOS version warnings — they're never actionable
      const warnings = stderr
        .split('\n')
        .filter(l => !l.includes('was built for newer macOS version'))
        .join('\n')
        .trim()

      if (err) return resolve({ ok: false, cmd, output: warnings })

      // pipe the cart's output back to the editor's runtime log panel.
      // not detached / not unref'd → the cart dies with the editor.
      const wc = _event.sender
      const send = (ch, payload) => { if (!wc.isDestroyed()) wc.send(ch, payload) }
      const proc = spawn(CART_BIN, [], { detached: false, stdio: ['ignore', 'pipe', 'pipe'], cwd: BUILD_DIR })
      proc.stdout.on('data', chunk => send('cart:log', chunk.toString()))   // raylib trace (warnings only) + stray printf
      proc.stderr.on('data', chunk => send('cart:log', chunk.toString()))   // printh() output
      proc.on('exit', (code, signal) => send('cart:exit', { code, signal }))
      proc.on('error', () => {})

      // also cross-compile for Windows in the background
      if (fs.existsSync(MINGW) || fs.existsSync(`/usr/local/bin/${MINGW}`)) {
        const winArgs = [
          `"${CART_SRC}"`, `"${studioC}"`,
          `-I"${RUNTIME_DIR}"`, `-I"${BUILD_DIR}"`, `-I"${RAYLIB_WIN}/include"`,
          `-DSCREEN_W=${screenW}`,
          `-DSCREEN_H=${screenH}`,
          `-DSCALE=${scale}`,
          `-DMAP_W=${mapW}`,
          `-DMAP_H=${mapH}`,
          `-DCELL_W=${cellW}`,
          `-DCELL_H=${cellH}`,
          `-DTOUCH_CONTROLS_DEFAULT=${touchDefault}`,
          '-Os',
          '-fno-delete-null-pointer-checks',
          `"${RAYLIB_WIN}/lib/libraylib.a"`,
          '-lopengl32', '-lgdi32', '-lwinmm', '-lcomdlg32',
          '-Wl,--gc-sections',
          `-o "${CART_EXE}"`,
        ]
        exec(`${MINGW} ${winArgs.join(' ')}`, () => {})
      }

      resolve({
        ok:     true,
        cmd,
        output: warnings || null,
        src:    CART_SRC,
        bin:    CART_BIN,
        exe:    CART_EXE,
      })
    })
  })
})

// ── web preview server ────────────────────────────────────────
const WEB_PORT = 8765
const WEB_MIME = { '.html': 'text/html', '.js': 'application/javascript', '.wasm': 'application/wasm', '.png': 'image/png' }
let webServer = null

function startWebServer() {
  const url = `http://localhost:${WEB_PORT}/cart.html`
  return new Promise((resolve, reject) => {
    if (webServer?.listening) return resolve(url)  // already up — new files on disk, just reuse it
    webServer = http.createServer((req, res) => {
      const filePath = path.join(BUILD_DIR, req.url === '/' ? 'cart.html' : req.url)
      fs.readFile(filePath, (err, data) => {
        if (err) { res.writeHead(404); res.end('not found'); return }
        res.writeHead(200, { 'Content-Type': WEB_MIME[path.extname(filePath)] || 'application/octet-stream' })
        res.end(data)
      })
    })
    webServer.listen(WEB_PORT, '127.0.0.1', () => resolve(url))
    webServer.on('error', reject)
  })
}

// ── build for web (emscripten) ────────────────────────────────
ipcMain.handle('studio:build-web', async (_event, code, cfg) => {
  const screenW      = cfg?.screenW || 320
  const screenH      = cfg?.screenH || 200
  const scale        = cfg?.scale   || 4
  const mapW         = cfg?.mapW    || 128
  const mapH         = cfg?.mapH    || 64
  const cellW        = cfg?.cellW   || 16
  const cellH        = cfg?.cellH   || 16
  const touchDefault = cfg?.touchControls ? 1 : 0
  const studioC      = path.join(RUNTIME_DIR, 'studio.c')
  const shellHtml    = path.join(RUNTIME_DIR, 'web_shell.html')
  const RAYLIB_WEB   = path.join(RUNTIME_DIR, 'raylib-web')
  const CART_HTML    = path.join(BUILD_DIR, 'cart.html')

  if (!fs.existsSync(path.join(RAYLIB_WEB, 'include', 'raylib.h'))) {
    return {
      ok: false,
      output: [
        'Raylib web library not found at runtime/raylib-web/.',
        'Download raylib-5.5_webassembly.zip from github.com/raysan5/raylib/releases',
        'and extract it there so runtime/raylib-web/include/raylib.h exists.',
        'Also install emscripten: brew install emscripten',
      ].join('\n'),
    }
  }

  const wc  = _event.sender
  const log = (msg) => { if (!wc.isDestroyed()) wc.send('cart:log', msg) }

  fs.mkdirSync(BUILD_DIR, { recursive: true })
  fs.writeFileSync(CART_SRC, code)

  log('── build for web ──\n')
  log('generating asset headers…\n')

  const spritesHeader = path.join(BUILD_DIR, 'sprites_data.h')
  const spritesPng    = path.join(BUILD_DIR, 'sprites.png')
  if (fs.existsSync(spritesPng)) {
    try {
      const xxd = execSync('xxd -i sprites.png', { cwd: BUILD_DIR }).toString()
      fs.writeFileSync(spritesHeader,
        xxd.replace(/unsigned char sprites_png\[\]/, 'static const unsigned char SPRITES_DATA[]')
           .replace(/unsigned int sprites_png_len/,  'static const unsigned int  SPRITES_DATA_LEN'))
    } catch {
      fs.writeFileSync(spritesHeader, 'static const unsigned char SPRITES_DATA[]={0};static const unsigned int SPRITES_DATA_LEN=0;\n')
    }
  } else {
    fs.writeFileSync(spritesHeader, 'static const unsigned char SPRITES_DATA[]={0};static const unsigned int SPRITES_DATA_LEN=0;\n')
  }

  const mapHeader = path.join(BUILD_DIR, 'map_data.h')
  const mapDat    = path.join(BUILD_DIR, 'map.dat')
  if (fs.existsSync(mapDat)) {
    try {
      const xxd = execSync('xxd -i map.dat', { cwd: BUILD_DIR }).toString()
      fs.writeFileSync(mapHeader,
        xxd.replace(/unsigned char map_dat\[\]/,  'static const unsigned char MAP_DATA[]')
           .replace(/unsigned int map_dat_len/,   'static const unsigned int  MAP_DATA_LEN'))
    } catch {
      fs.writeFileSync(mapHeader, 'static const unsigned char MAP_DATA[]={0};static const unsigned int MAP_DATA_LEN=0;\n')
    }
  } else {
    fs.writeFileSync(mapHeader, 'static const unsigned char MAP_DATA[]={0};static const unsigned int MAP_DATA_LEN=0;\n')
  }

  const args = [
    `"${CART_SRC}"`,
    `"${studioC}"`,
    `-I"${RUNTIME_DIR}"`,
    `-I"${BUILD_DIR}"`,
    `-I"${RAYLIB_WEB}/include"`,
    '-DPLATFORM_WEB',
    `-DSCREEN_W=${screenW}`,
    `-DSCREEN_H=${screenH}`,
    `-DSCALE=${scale}`,
    `-DMAP_W=${mapW}`,
    `-DMAP_H=${mapH}`,
    `-DCELL_W=${cellW}`,
    `-DCELL_H=${cellH}`,
    `-DTOUCH_CONTROLS_DEFAULT=${touchDefault}`,
    '-Os',
    '-fno-delete-null-pointer-checks',
    `"${RAYLIB_WEB}/lib/libraylib.a"`,
    '-s USE_GLFW=3',
    '-s TOTAL_MEMORY=67108864',
    '-s EXPORTED_RUNTIME_METHODS=ccall,HEAPF32',
    `--shell-file "${shellHtml}"`,
    `-o "${CART_HTML}"`,
  ]

  const cmd = `emcc ${args.join(' ')}`
  log('running emcc… (this takes ~10s)\n')

  return new Promise(resolve => {
    exec(cmd, { timeout: 120000 }, async (err, _stdout, stderr) => {
      if (err) {
        log(stderr.trim() || err.message)
        return resolve({ ok: false, cmd, output: stderr.trim() || err.message })
      }
      const url = await startWebServer()
      shell.openExternal(url)
      log(`✓ done — opening ${url}\n`)
      resolve({ ok: true, cmd, url, output: null })
    })
  })
})
