const { app, BrowserWindow, ipcMain, Menu, session, dialog, shell, nativeImage } = require('electron')
const { exec, execSync, spawn }        = require('child_process')
const path                             = require('path')
const fs                               = require('fs')
const zlib                             = require('zlib')
const http                             = require('http')

// Internal app name (menu bar, userData path). The Cmd-Tab / Dock *label* in dev
// comes from the Electron bundle's Info.plist, not this — see scripts/dev-branding.cjs.
app.setName('dreamengine')

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

// parse the de:settings JSON chunk into an object, or null if absent/invalid
function parseCartSettings(raw) {
  if (!raw) return null
  try { return JSON.parse(raw) } catch { return null }
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
// live (libtcc) backend: a persistent -DDE_TCC host that JIT-compiles cart.c and
// hot-reloads it on save. See docs/design/cart-as-script.md.
const LIBTCC_DIR  = path.join(RUNTIME_DIR, 'libtcc')
const TCC_HOST_BIN = path.join(BUILD_DIR, 'cart_live_host')

// ── cart build prep (shared by run + profile) ─────────────────
// Resolve config → dims, regenerate the embedded data headers, write cart.c.
function prepareCart(code, cfg) {
  const dims = {
    screenW: cfg?.screenW || 320,
    screenH: cfg?.screenH || 200,
    scale:   cfg?.scale   || 4,
    mapW:    cfg?.mapW    || 128,
    mapH:    cfg?.mapH    || 64,
    cellW:   cfg?.cellW   || 16,
    cellH:   cfg?.cellH   || 16,
    touchDefault: cfg?.touchControls ? 1 : 0,
    keymap:  cfg?.keymap || null,
    studioC: path.join(RUNTIME_DIR, 'studio.c'),
  }

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

  // generate map_data.h from map.dat (raw bytes = MAP_W × MAP_H cell indices)
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

  return dims
}

// Turn the editor's saved keymap into -DP0_BTN_A=<keycode> … flags. Omitted
// buttons just fall back to studio.c's compiled-in defaults, so a null/partial
// keymap is safe. Shared by every native build path (run / profile / live host).
function keymapDefs(keymap) {
  if (!keymap) return []
  const btns = [['up', 'UP'], ['down', 'DOWN'], ['left', 'LEFT'], ['right', 'RIGHT'], ['a', 'A'], ['b', 'B']]
  const out = []
  for (const [pid, prefix] of [['p0', 'P0'], ['p1', 'P1']]) {
    const m = keymap[pid]
    if (!m) continue
    for (const [btn, suffix] of btns) {
      const code = m[btn]
      if (Number.isInteger(code)) out.push(`-D${prefix}_BTN_${suffix}=${code}`)
    }
  }
  if (Number.isInteger(keymap.pause)) out.push(`-DPAUSE_KEY=${keymap.pause}`)
  return out
}

// macOS clang command for a native cart build. optFlags swaps between the
// shipping build (-Os) and the profiling build (-O1 -fno-inline, which keeps
// studio.c primitives as distinct, named symbols so `sample` can attribute
// time to them instead of folding them into draw()).
function macCompileArgs(dims, optFlags) {
  const { screenW, screenH, scale, mapW, mapH, cellW, cellH, touchDefault, keymap, studioC } = dims
  return [
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
    ...keymapDefs(keymap),
    ...optFlags,
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
}

// clang command for the LIVE (libtcc) host: studio.c compiled with -DDE_TCC and linked
// against the vendored libtcc.dylib. The cart is NOT linked in — the host JIT-loads
// cart.c at runtime and hot-reloads it on save. Screen/map dims are baked into the host
// (it forwards them to the cart), so a dims change means a host rebuild.
function macTccHostArgs(dims) {
  const { screenW, screenH, scale, mapW, mapH, cellW, cellH, touchDefault, keymap } = dims
  return [
    `"${path.join(RUNTIME_DIR, 'studio.c')}"`,
    '-DDE_TCC',
    `-DDE_TCC_INCDIR='"${RUNTIME_DIR}"'`,
    `-DDE_TCC_LIBDIR='"${path.join(LIBTCC_DIR, 'tcclib')}"'`,
    `-I"${RUNTIME_DIR}"`,
    `-I"${LIBTCC_DIR}"`,
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
    ...keymapDefs(keymap),
    '-fno-delete-null-pointer-checks',
    `"${path.join(LIBTCC_DIR, 'libtcc.dylib')}"`,
    `-Wl,-rpath,"${LIBTCC_DIR}"`,
    `"${RAYLIB}/lib/libraylib.a"`,
    '-framework OpenGL', '-framework Cocoa', '-framework IOKit',
    '-framework CoreVideo', '-framework CoreFoundation',
    '-lm', '-ldl', '-lpthread',
    `-o "${TCC_HOST_BIN}"`,
  ]
}

// Live-host process + the signature of the build it was launched for. While the host
// runs, a code-only edit just rewrites cart.c (its file-watch hot-reloads in place);
// a change to studio.c/.h, the symbol table, screen dims, sprites or map invalidates
// the baked-in host and forces a rebuild + relaunch.
let liveProc = null
let liveSig  = null
function fileMtime(p) { try { return fs.statSync(p).mtimeMs } catch { return 0 } }
function fileHash(p)  { try { return require('crypto').createHash('md5').update(fs.readFileSync(p)).digest('hex') } catch { return '' } }
function liveSignature(dims) {
  // What forces a host REBUILD/relaunch (vs. an in-place hot reload). studio sources +
  // dims are baked into the host binary; so are sprites/map (via the embedded data
  // headers). BUT the editor rewrites sprites.png/map.dat on every run, so keying on
  // their mtime would relaunch the window every time — key on their CONTENT instead, so
  // an unchanged sheet keeps the same signature and the cart hot-reloads in place.
  return JSON.stringify({
    dims,
    studio:  fileMtime(path.join(RUNTIME_DIR, 'studio.c')),
    studioH: fileMtime(path.join(RUNTIME_DIR, 'studio.h')),
    syms:    fileMtime(path.join(RUNTIME_DIR, 'studio_tcc_symbols.h')),
    sprites: fileHash(path.join(BUILD_DIR, 'sprites.png')),
    map:     fileHash(path.join(BUILD_DIR, 'map.dat')),
  })
}
function killLiveHost() {
  // mark the kill as ours so its SIGTERM exit isn't reported to the editor as a crash
  if (liveProc && !liveProc.killed) { liveProc.__intentional = true; try { liveProc.kill() } catch {} }
  liveProc = null
}
function buildTccHost(dims) {
  return new Promise(resolve => {
    if (!fs.existsSync(path.join(LIBTCC_DIR, 'libtcc.dylib'))) {
      return resolve({ ok: false, cmd: '', output: `live backend needs runtime/libtcc/ — not found.\nSee runtime/libtcc/README.md.` })
    }
    const cmd = `clang ${macTccHostArgs(dims).join(' ')}`
    exec(cmd, (err, _o, stderr) => {
      const output = stderr.split('\n').filter(l => !l.includes('was built for newer macOS version')).join('\n').trim()
      resolve({ ok: !err, cmd, output })
    })
  })
}

// LIVE backend: spawn the persistent libtcc host once; thereafter a Run just rewrites
// cart.c and the host hot-reloads it (state in de_state() survives). prepareCart() has
// already written cart.c + the data headers before we get here.
async function runLive(dims, cfg, wc) {
  const send = (ch, payload) => { if (!wc.isDestroyed()) wc.send(ch, payload) }
  const sig  = liveSignature(dims)

  // host already up for this exact build → cart.c was just rewritten; file-watch reloads.
  if (liveProc && !liveProc.killed && sig === liveSig) {
    send('cart:log', '↻ live reload\n')
    return { ok: true, live: true, reloaded: true, src: CART_SRC }
  }

  const wasRunning = !!(liveProc && !liveProc.killed)   // true → a relaunch, not a first launch
  killLiveHost()
  if (!fs.existsSync(TCC_HOST_BIN) || sig !== liveSig) {
    send('cart:log', '── building live host ──\n')
    const b = await buildTccHost(dims)
    if (!b.ok) return { ok: false, cmd: b.cmd, output: b.output || 'live host build failed' }
  }
  if (wasRunning) send('cart:log', '↻ relaunching live window (sprites / screen / runtime changed)\n')
  liveSig = sig

  const cartTitle = cfg?.cartName ? `dreamengine (live): ${cfg.cartName}` : 'dreamengine (live)'
  const proc = spawn(TCC_HOST_BIN, ['--cart', CART_SRC, '--title', cartTitle],
    { detached: false, stdio: ['ignore', 'pipe', 'pipe'], cwd: BUILD_DIR })
  proc.stdout.on('data', c => send('cart:log', c.toString()))
  proc.stderr.on('data', c => send('cart:log', c.toString()))   // [tcc] compiled / hot-reloaded / errors
  proc.on('exit', (code, signal) => {
    const wasIntentional = proc.__intentional
    if (proc === liveProc) liveProc = null
    if (!wasIntentional) send('cart:exit', { code, signal })   // real exit (window closed / crash)
  })
  proc.on('error', () => {})
  liveProc = proc
  return { ok: true, live: true, src: CART_SRC, bin: TCC_HOST_BIN }
}

// Public studio.h function names — the cart author's vocabulary. These are the
// roll-up targets for call-path attribution. Read from the header (so new
// primitives are picked up automatically) and cached.
let _prims = null
function studioPrimitives() {
  if (_prims) return _prims
  _prims = new Set(['draw', 'update', 'init'])
  try {
    const h = fs.readFileSync(path.join(RUNTIME_DIR, 'studio.h'), 'utf8')
    for (const m of h.matchAll(/^\s*(?:void|int|bool|float)\s+([a-z_][a-z0-9_]*)\s*\(/gm)) _prims.add(m[1])
  } catch {}
  return _prims
}

// Parse a `sample` report → cart-code CPU hotspots WITH the call paths that
// reach them. We walk the call graph: a node's `count` is inclusive (node +
// descendants), so exclusive = count − Σ children. Exclusive time is credited
// to its leaf symbol (the hot end function, e.g. rlVertex3f) AND tagged with
// the path up to the owning studio.h primitive — so the author sees both what's
// expensive and which of their calls (trifill/circfill/…) drives it.
// Only cart-binary symbols count; dynamic frameworks (OpenGL/libsystem) are the
// vsync/idle wait we ignore. Percentages are relative to cart-active samples.
function parseSample(report) {
  if (!report) return { total: 0, cartSamples: 0, leaves: [] }

  // denominator = main-thread samples (raylib's ~10 idle worker/audio threads
  // would otherwise bury cart code). Fall back to the busiest thread root.
  let total = 0
  const mainM = report.match(/^\s+(\d+)\s+Thread_\S+:\s*Main Thread/m)
  if (mainM) total = parseInt(mainM[1])
  else for (const m of report.matchAll(/^\s+(\d+)\s+Thread_/gm)) total = Math.max(total, parseInt(m[1]))

  const procName = (report.match(/^Process:\s+(\S+)/m) || [])[1] || 'cart'
  const PRIM = studioPrimitives()

  // build the call-graph tree — indent width is proportional to depth
  const section = report.slice(report.indexOf('Call graph:'))
  const endIdx  = section.indexOf('Total number in stack')
  const lines   = (endIdx > 0 ? section.slice(0, endIdx) : section).split('\n')
  const nodeRe  = /^([+!:|\s]*?)(\d+)\s+(.+?)\s+\(in ([^)]+)\)/
  const stack = []
  const nodes = []
  for (const line of lines) {
    const m = line.match(nodeRe)
    if (!m) continue
    const node = { indent: m[1].length, count: parseInt(m[2]), symbol: m[3].trim(), binary: m[4], children: [], parent: null }
    while (stack.length && stack[stack.length - 1].indent >= node.indent) stack.pop()
    if (stack.length) { node.parent = stack[stack.length - 1]; node.parent.children.push(node) }
    stack.push(node)
    nodes.push(node)
  }

  // exclusive samples per node → tally by leaf symbol, and by leaf + call path
  const leafSamples = {}        // symbol -> total exclusive
  const pathSamples = {}        // symbol -> { pathStr -> exclusive }
  for (const n of nodes) {
    if (n.binary !== procName) continue
    const childSum = n.children.reduce((s, c) => s + c.count, 0)
    const excl = Math.max(0, n.count - childSum)
    if (!excl) continue

    // full chain root→leaf, then find the OUTERMOST studio.h primitive in it.
    // - no primitive at all → engine plumbing (the vsync-wait/frame-limiter path
    //   loop_step›EndDrawing›WaitTime›glfwGetTime, the canvas blit, the audio
    //   thread): not the cart's frame work, so skip it.
    // - root the displayed path at that outermost primitive so the cart's OWN
    //   functions (draw › draw_rag › …) stay visible instead of being chopped
    //   off above the nearest drawing primitive.
    const chain = []
    for (let a = n; a; a = a.parent) chain.unshift(a.symbol)
    const rootPrim = chain.findIndex(s => PRIM.has(s))
    if (rootPrim < 0) continue

    leafSamples[n.symbol] = (leafSamples[n.symbol] || 0) + excl
    const sub = chain.slice(rootPrim)
    const via = sub.length > 5 ? '…› ' + sub.slice(-5).join(' › ') : sub.join(' › ')
    const byPath = pathSamples[n.symbol] || (pathSamples[n.symbol] = {})
    byPath[via] = (byPath[via] || 0) + excl
  }

  const cartSamples = Object.values(leafSamples).reduce((s, v) => s + v, 0)
  const leaves = Object.entries(leafSamples)
    .sort((a, b) => b[1] - a[1])
    .map(([symbol, samples]) => ({
      symbol,
      samples,
      pct: cartSamples ? (samples / cartSamples) * 100 : 0,
      paths: Object.entries(pathSamples[symbol] || {})
        .sort((a, b) => b[1] - a[1])
        .slice(0, 3)
        .map(([via, s]) => ({ via, samples: s }))
        .filter(p => p.via !== symbol),   // drop the redundant self-path
    }))

  return { total, cartSamples, leaves }
}

// Read the in-engine perf.json the profiling build (-DDE_PROFILE) writes: frame
// CPU timing (C) + per-primitive draw-call counts (D). Returns null if absent.
function readPerf() {
  try {
    const j = JSON.parse(fs.readFileSync(path.join(BUILD_DIR, 'perf.json'), 'utf8'))
    const frames = j.frames || 1
    // merge by name (safety net in case identical string literals weren't pooled)
    const byName = {}
    for (const c of j.calls || []) byName[c.name] = (byName[c.name] || 0) + c.total
    const calls = Object.entries(byName)
      .map(([name, total]) => ({ name, perFrame: total / frames, total }))
      .sort((a, b) => b.perFrame - a.perFrame)
    const frameMsAvg = j.frameMsAvg || 0
    // Robust per-frame stats: a single stall (e.g. the frame `sample` attaches
    // and suspends our threads) wrecks both the raw max AND the mean — for a
    // near-idle cart one 125ms frame can be half the average. So headline the
    // median ("typical frame") + p95, both of which ignore the tail.
    const work = (j.work || []).slice().sort((a, b) => a - b)
    const pct = q => work.length ? work[Math.min(work.length - 1, Math.floor(work.length * q))] : (j.workMsMax || 0)
    const median = pct(0.50)
    return {
      frames,
      workMedian: median,
      workP95:    pct(0.95),
      workMsAvg:  j.workMsAvg || 0,   // kept for reference; not displayed (outlier-sensitive)
      workMsMax:  work.length ? work[work.length - 1] : (j.workMsMax || 0),
      frameMsAvg,
      fps:        frameMsAvg > 0 ? 1000 / frameMsAvg : 0,
      budgetPct:  median / (1000 / 60) * 100,   // typical frame's share of the 16.6ms 60fps budget
      calls,
    }
  } catch { return null }
}

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
  // Dock/taskbar icon — the magic-pink run-button square (#ff6fb5).
  // In dev (`electron .`) macOS otherwise shows the default Electron icon.
  if (app.dock) {
    const icon = nativeImage.createFromPath(path.join(__dirname, 'icon.png'))
    if (!icon.isEmpty()) app.dock.setIcon(icon)
  }
  session.defaultSession.setPermissionRequestHandler((_wc, permission, callback) => {
    callback(permission === 'clipboard-read' || permission === 'clipboard-write' || permission === 'clipboard-sanitized-write')
  })
  session.defaultSession.setPermissionCheckHandler((_wc, permission) => {
    return permission === 'clipboard-read' || permission === 'clipboard-write' || permission === 'clipboard-sanitized-write'
  })
  buildAppMenu('')
  createWindow()
})

// The app menu is rebuilt whenever the loaded cart changes, so the macOS menu
// bar shows what's open (" dreamengine  Edit  View  <cart>") — the window's own
// titlebar is hidden (hiddenInset), so this is the visible "what am I editing".
function buildAppMenu(cartName) {
  const template = [
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
  ]
  if (cartName) {
    // macOS top-level menus must have a submenu; this one is informational
    template.push({
      label: cartName,
      submenu: [{ label: 'loaded cart', enabled: false }],
    })
  }
  Menu.setApplicationMenu(Menu.buildFromTemplate(template))
}

// renderer tells us the loaded cart's name (shell.js setCartName)
ipcMain.on('cart:set-name', (_event, name) => {
  buildAppMenu(typeof name === 'string' ? name : '')
})

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') app.quit()
})


// ── cart save ─────────────────────────────────────────────────
ipcMain.handle('cart:save', async (_event, { source, spritesDataUrl, mapBase64, settings }) => {
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

  const chunkData = { source, sprites: spritesDataUrl, map: mapBase64 }
  if (settings) chunkData.settings = JSON.stringify(settings)
  const cartPng = embedCartChunks(basePng, chunkData)
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
  const name = path.basename(filePaths[0]).replace(/\.cart\.png$/i, '')
  return { ok: true, name, source: chunks.source, spritesDataUrl: chunks.sprites || null, mapBase64: chunks.map || null, settings: parseCartSettings(chunks.settings) }
})

ipcMain.handle('cart:load-buffer', async (_event, bytes) => {
  const chunks = extractCartChunks(Buffer.from(bytes))
  if (!chunks.source) return { ok: false, error: 'Not a dreamengine cart' }
  return { ok: true, source: chunks.source, spritesDataUrl: chunks.sprites || null, mapBase64: chunks.map || null, settings: parseCartSettings(chunks.settings) }
})

ipcMain.handle('cart:load-file', async (_event, filePath) => {
  const chunks = extractCartChunks(fs.readFileSync(filePath))
  if (!chunks.source) return { ok: false, error: 'Not a dreamengine cart' }
  const name = path.basename(filePath).replace(/\.cart\.png$/i, '')
  return { ok: true, name, source: chunks.source, spritesDataUrl: chunks.sprites || null, mapBase64: chunks.map || null, settings: parseCartSettings(chunks.settings) }
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
  const dims = prepareCart(code, cfg)

  // live (libtcc) backend — JIT + hot reload, no static link / no Windows build
  if (cfg?.backend === 'live') return runLive(dims, cfg, _event.sender)
  killLiveHost()   // a native run supersedes any running live host

  const { screenW, screenH, scale, mapW, mapH, cellW, cellH, touchDefault, keymap, studioC } = dims

  const args = macCompileArgs(dims, ['-Os'])
  const cmd  = `clang ${args.join(' ')}`

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
      const cartTitle = cfg?.cartName ? `dreamengine: ${cfg.cartName}` : 'dreamengine'
      const proc = spawn(CART_BIN, ['--title', cartTitle], { detached: false, stdio: ['ignore', 'pipe', 'pipe'], cwd: BUILD_DIR })
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
          ...keymapDefs(keymap),
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

// LIVE auto-reload: while a live host is up, the editor calls this on a debounce as you
// type. It just rewrites cart.c — the host's file-watch recompiles + hot-reloads it (a
// bad edit keeps the last good cart running). No sprite re-export, no host rebuild.
// No-op if no live host is running, so typing never launches anything on its own.
ipcMain.handle('studio:live-write', (_event, code) => {
  if (!liveProc || liveProc.killed) return { ok: false, idle: true }
  try { fs.writeFileSync(CART_SRC, code); return { ok: true } }
  catch (e) { return { ok: false, error: String(e) } }
})

// ── profile a cart ────────────────────────────────────────────
// Compile a profiling build (-O1 -fno-inline, named symbols), launch it,
// give it a moment to settle, then attach macOS `sample` for a few seconds —
// no Instruments GUI. The cart's own window shows briefly and is killed when
// sampling ends. Returns ranked cart-code CPU hotspots for the build log.
ipcMain.handle('studio:profile', async (_event, code, cfg) => {
  const dims = prepareCart(code, cfg)
  const seconds = cfg?.profileSeconds || 4

  const args = macCompileArgs(dims, ['-O1', '-fno-inline', '-fno-omit-frame-pointer', '-DDE_PROFILE'])
  const cmd  = `clang ${args.join(' ')}`

  return new Promise(resolve => {
    exec(cmd, (err, _stdout, stderr) => {
      const warnings = stderr
        .split('\n')
        .filter(l => !l.includes('was built for newer macOS version'))
        .join('\n')
        .trim()

      if (err) return resolve({ ok: false, profile: true, cmd, output: warnings })

      const wc = _event.sender
      const send = (ch, payload) => { if (!wc.isDestroyed()) wc.send(ch, payload) }
      const cartTitle = cfg?.cartName ? `dreamengine: ${cfg.cartName} (profiling)` : 'dreamengine (profiling)'

      // clear any stale perf.json so a crash-before-first-flush can't show old data
      try { fs.unlinkSync(path.join(BUILD_DIR, 'perf.json')) } catch {}

      const proc = spawn(CART_BIN, ['--title', cartTitle], { detached: false, stdio: ['ignore', 'pipe', 'pipe'], cwd: BUILD_DIR })
      let exited = false
      proc.stdout.on('data', chunk => send('cart:log', chunk.toString()))
      proc.stderr.on('data', chunk => send('cart:log', chunk.toString()))
      proc.on('exit', (codeN, signal) => { exited = true; send('cart:exit', { code: codeN, signal }) })
      proc.on('error', () => { exited = true })

      const sampleFile = path.join(BUILD_DIR, 'profile.txt')

      // let the window open and start drawing before we start sampling
      setTimeout(() => {
        if (exited || !proc.pid) {
          return resolve({ ok: false, profile: true, cmd, output: 'cart exited before profiling could start' })
        }
        exec(`/usr/bin/sample ${proc.pid} ${seconds} -file "${sampleFile}"`, sErr => {
          try { proc.kill() } catch {}
          if (sErr) {
            return resolve({ ok: false, profile: true, cmd, output: `sample failed: ${sErr.message}` })
          }
          let report = ''
          try { report = fs.readFileSync(sampleFile, 'utf8') } catch {}
          resolve({ ok: true, profile: true, cmd, seconds, hotspots: parseSample(report), perf: readPerf() })
        })
      }, 1000)
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
