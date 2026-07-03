const { app, BrowserWindow, ipcMain, Menu, session, dialog, shell, nativeImage } = require('electron')
const { exec, execSync, spawn }        = require('child_process')
const path                             = require('path')
const fs                               = require('fs')
const zlib                             = require('zlib')
const http                             = require('http')
const os                               = require('os')

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
    // skip any pre-existing de:* zTXt chunks so a re-embed (save-in-place onto
    // an existing cart PNG) replaces them instead of appending duplicates
    let isDeChunk = false
    if (type === 'zTXt') {
      const body = pngBuf.slice(offset + 8, offset + 8 + len)
      const nullIdx = body.indexOf(0)
      if (nullIdx !== -1 && body.slice(0, nullIdx).toString('latin1').startsWith('de:')) isDeChunk = true
    }
    if (!isDeChunk) parts.push(chunk)
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
// the shared gallery registry directory — carts loaded from here have NO
// save-in-place origin (they're build products of tools/carts/ AND the
// multi-agent-hazard registry the editor never writes; see CLAUDE.md hazard #2)
const GALLERY_DIR = path.join(__dirname, '../public/carts')
function inGallery(p) {
  const rel = path.relative(GALLERY_DIR, path.resolve(p))
  return rel && !rel.startsWith('..') && !path.isAbsolute(rel)
}
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
// libtcc.h is shared; the arch-specific binaries (libtcc.dylib + tcclib/libtcc1.a) live
// in a per-arch subdir (arm64/ or x64/) because the dylib bakes in the macOS SDK include
// path at build time — see runtime/libtcc/README.md.
const LIBTCC_DIR     = path.join(RUNTIME_DIR, 'libtcc')
const LIBTCC_ARCH_DIR = path.join(LIBTCC_DIR, process.arch)   // 'arm64' | 'x64'
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
    scaleFilter: cfg?.scaleFilter || 0,
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
  const btns = [['up', 'UP'], ['down', 'DOWN'], ['left', 'LEFT'], ['right', 'RIGHT'], ['a', 'A'], ['b', 'B'], ['x', 'X'], ['y', 'Y']]
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

// Per-cart save folder: --save-dir saves/<slug> (relative to cwd=build/), so
// cart.sav / cart.kv / cart.blob aren't shared by every cart that runs from
// build/. Unsaved scratch code shares the 'scratch' folder. Shared by all
// three spawn paths (run / profile / live host).
function saveDirArgs(cfg) {
  const slug = (cfg?.cartName || 'scratch').toLowerCase()
    .replace(/[^a-z0-9]+/g, '-').replace(/^-+|-+$/g, '') || 'scratch'
  return ['--save-dir', `saves/${slug}`]
}

// Lockstep netplay flags for a run (rung 2 — docs/design/multiplayer-research.md).
// cfg.net is { mode:'host' } or { mode:'join', ip }; anything else = solo run.
function netArgs(cfg) {
  const n = cfg?.net
  if (!n) return []
  if (n.mode === 'host') return ['--net-host']
  if (n.mode === 'join' && n.ip) return ['--net-join', String(n.ip)]
  return []
}

// This machine's best-guess LAN IPv4, to show the host so the joiner knows what
// to type. Mirrors net.h's net_local_ipv4(): prefer a private range, skip
// loopback / link-local (169.254). Node's os.networkInterfaces() already flags
// internal + family, so this stays short.
function lanIPv4() {
  const ifs = os.networkInterfaces()
  let fallback = null
  for (const addrs of Object.values(ifs)) {
    for (const a of addrs || []) {
      if (a.family !== 'IPv4' || a.internal) continue
      if (a.address.startsWith('169.254.')) continue
      if (a.address.startsWith('192.168.') || a.address.startsWith('10.')) return a.address
      if (!fallback) fallback = a.address
    }
  }
  return fallback || '127.0.0.1'
}

// Environment for a spawned cart. The render backend (GPU vs software canvas) is a
// runtime env var the studio reads at startup (studio.c: DE_SOFTWARE_CANVAS), so no
// recompile is needed to switch — just relaunch. settings.renderMode === 'software'
// flips it on. NOTE: in live mode this only takes effect on a host RELAUNCH, so
// renderMode is folded into liveSignature() to force one when it changes.
function cartEnv(cfg) {
  return { ...process.env, DE_SOFTWARE_CANVAS: cfg?.renderMode === 'software' ? 'on' : 'off' }
}

// macOS clang command for a native cart build. optFlags swaps between the
// shipping build (-Os) and the profiling build (-O1 -fno-inline, which keeps
// studio.c primitives as distinct, named symbols so `sample` can attribute
// time to them instead of folding them into draw()).
// Bake a window title into an exported binary (double-clicked apps get no argv, so
// the --title flag can't reach them). Quoted for the sh -c line exec() runs through.
function titleDef(name) {
  const safe = String(name || '').replace(/["'\\$`]/g, '').trim()
  return safe ? [`-DDE_WINDOW_TITLE='"${safe}"'`] : []
}

function macCompileArgs(dims, optFlags, out = CART_BIN, extraDefs = []) {
  const { screenW, screenH, scale, mapW, mapH, cellW, cellH, touchDefault, scaleFilter, keymap, studioC } = dims
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
    `-DSCALE_FILTER=${scaleFilter}`,
    ...keymapDefs(keymap),
    ...extraDefs,
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
    '-framework CoreMIDI',
    '-Wl,-dead_strip',
    `-o "${out}"`,
  ]
}

// MinGW cross-compile args for a Windows .exe. `out` is the output path; opts:
//   lobby → -DDE_NET_LOBBY_DEFAULT (double-click boots into the multiplayer lobby)
//   gui   → -mwindows (GUI subsystem: no console window pops up alongside the game)
// Shared by the background build (studio:run) and the Export .exe button.
function winCompileArgs(dims, out, opts = {}) {
  const { screenW, screenH, scale, mapW, mapH, cellW, cellH, touchDefault, keymap, studioC } = dims
  return [
    `"${CART_SRC}"`, `"${studioC}"`,
    `-I"${RUNTIME_DIR}"`, `-I"${BUILD_DIR}"`, `-I"${RAYLIB_WIN}/include"`,
    `-DSCREEN_W=${screenW}`, `-DSCREEN_H=${screenH}`, `-DSCALE=${scale}`,
    `-DMAP_W=${mapW}`, `-DMAP_H=${mapH}`, `-DCELL_W=${cellW}`, `-DCELL_H=${cellH}`,
    `-DTOUCH_CONTROLS_DEFAULT=${touchDefault}`,
    ...keymapDefs(keymap),
    ...(opts.lobby ? ['-DDE_NET_LOBBY_DEFAULT'] : []),
    ...titleDef(opts.title),
    '-Os', '-fno-delete-null-pointer-checks',
    `"${RAYLIB_WIN}/lib/libraylib.a"`,
    '-lopengl32', '-lgdi32', '-lwinmm', '-lcomdlg32', '-lws2_32',   // ws2_32 = Winsock, for net.h netplay
    ...(opts.gui ? ['-mwindows'] : []),
    '-Wl,--gc-sections',
    `-o "${out}"`,
  ]
}

function mingwAvailable() {
  return fs.existsSync(MINGW) || fs.existsSync(`/usr/local/bin/${MINGW}`) || fs.existsSync(`/opt/homebrew/bin/${MINGW}`)
}

// clang command for the LIVE (libtcc) host: studio.c compiled with -DDE_TCC and linked
// against the vendored libtcc.dylib. The cart is NOT linked in — the host JIT-loads
// cart.c at runtime and hot-reloads it on save. Screen/map dims are baked into the host
// (it forwards them to the cart), so a dims change means a host rebuild.
function macTccHostArgs(dims) {
  const { screenW, screenH, scale, mapW, mapH, cellW, cellH, touchDefault, scaleFilter, keymap } = dims
  return [
    `"${path.join(RUNTIME_DIR, 'studio.c')}"`,
    '-DDE_TCC',
    `-DDE_TCC_INCDIR='"${RUNTIME_DIR}"'`,
    `-DDE_TCC_LIBDIR='"${path.join(LIBTCC_ARCH_DIR, 'tcclib')}"'`,
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
    `-DSCALE_FILTER=${scaleFilter}`,
    ...keymapDefs(keymap),
    '-fno-delete-null-pointer-checks',
    `"${path.join(LIBTCC_ARCH_DIR, 'libtcc.dylib')}"`,
    `-Wl,-rpath,"${LIBTCC_ARCH_DIR}"`,
    `"${RAYLIB}/lib/libraylib.a"`,
    '-framework OpenGL', '-framework Cocoa', '-framework IOKit',
    '-framework CoreVideo', '-framework CoreFoundation', '-framework CoreMIDI',
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
function liveSignature(dims, cfg) {
  // What forces a host REBUILD/relaunch (vs. an in-place hot reload). studio sources +
  // dims are baked into the host binary; so are sprites/map (via the embedded data
  // headers). BUT the editor rewrites sprites.png/map.dat on every run, so keying on
  // their mtime would relaunch the window every time — key on their CONTENT instead, so
  // an unchanged sheet keeps the same signature and the cart hot-reloads in place.
  // renderMode rides along: the GPU/software choice is an env var read only at host
  // startup, so changing it must relaunch the host rather than hot-reload in place.
  return JSON.stringify({
    dims,
    renderMode: cfg?.renderMode || 'gpu',
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
    if (!fs.existsSync(path.join(LIBTCC_ARCH_DIR, 'libtcc.dylib'))) {
      return resolve({ ok: false, cmd: '', output: `live backend needs runtime/libtcc/${process.arch}/libtcc.dylib — not found.\nSee runtime/libtcc/README.md.` })
    }
    // re-derive the libtcc symbol list from studio.h before every host build,
    // so a new API function can never be missing from the live backend
    try { require('../../tools/gen-tcc-symbols.js').generate() } catch (e) { console.error('gen-tcc-symbols failed:', e.message) }
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
  const sig  = liveSignature(dims, cfg)

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

  const cartTitle = cfg?.cartName ? `${cfg.cartName} (live)` : 'dreamengine (live)'
  const proc = spawn(TCC_HOST_BIN, ['--cart', CART_SRC, '--title', cartTitle, ...saveDirArgs(cfg)],
    { detached: false, stdio: ['ignore', 'pipe', 'pipe'], cwd: BUILD_DIR, env: cartEnv(cfg) })
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

  // wait-on lets Electron through the instant the port answers, but on a cold
  // start Vite is still prebundling deps (esbuild) — the page's module requests
  // 504 mid-optimize and the renderer stays black forever. Retry the load until
  // Vite is genuinely ready. Bites slow CPUs hardest (wider optimize window).
  win.webContents.on('did-fail-load', (_e, _code, _desc, url) => {
    if (url && url.startsWith('http://localhost:5173')) {
      setTimeout(() => win.loadURL('http://localhost:5173'), 400)
    }
  })

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
// `targetPath` set → save-in-place (no dialog, keep the origin's baked
// thumbnail); absent → Save As (dialog, fresh thumbnail from the last run).
ipcMain.handle('cart:save', async (_event, { source, spritesDataUrl, mapBase64, settings, targetPath }) => {
  let filePath = targetPath
  let basePng
  if (filePath) {
    // save-in-place: re-embed chunks onto the EXISTING file, preserving its
    // visible (baked) thumbnail — a quick text edit must not swap the screenshot
    if (!fs.existsSync(filePath)) return { ok: false, error: 'origin file is gone' }
    basePng = fs.readFileSync(filePath)
  } else {
    const res = await dialog.showSaveDialog({
      title: 'Save Cart As',
      defaultPath: 'mycart.cart.png',
      filters: [{ name: 'Dreamengine Cart', extensions: ['png'] }],
    })
    if (!res.filePath) return { ok: false }
    filePath = res.filePath
    const screenshotPng = path.join(BUILD_DIR, 'screenshot.png')
    const spritesPng    = path.join(BUILD_DIR, 'sprites.png')
    basePng = fs.existsSync(screenshotPng)
      ? fs.readFileSync(screenshotPng)
      : fs.existsSync(spritesPng)
        ? fs.readFileSync(spritesPng)
        : Buffer.from(spritesDataUrl.replace(/^data:image\/png;base64,/, ''), 'base64')
  }

  const chunkData = { source, sprites: spritesDataUrl, map: mapBase64 }
  if (settings) chunkData.settings = JSON.stringify(settings)
  const cartPng = embedCartChunks(basePng, chunkData)
  fs.writeFileSync(filePath, cartPng)
  // a Save As into the gallery dir still confers no in-place origin
  return { ok: true, filePath, origin: inGallery(filePath) ? null : filePath, inPlace: !!targetPath }
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
  return { ok: true, name, origin: inGallery(filePaths[0]) ? null : filePaths[0], source: chunks.source, spritesDataUrl: chunks.sprites || null, mapBase64: chunks.map || null, settings: parseCartSettings(chunks.settings) }
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
  return { ok: true, name, origin: inGallery(filePath) ? null : filePath, source: chunks.source, spritesDataUrl: chunks.sprites || null, mapBase64: chunks.map || null, settings: parseCartSettings(chunks.settings) }
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

  const optFlags = cfg?.buildMode === 'release'
    ? ['-O2', '-DDE_RELEASE']
    : ['-Os']
  const args = macCompileArgs(dims, optFlags)
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
      const cartTitle = cfg?.cartName || 'dreamengine'
      const netFlags  = netArgs(cfg)
      const proc = spawn(CART_BIN, ['--title', cartTitle, ...saveDirArgs(cfg), ...netFlags], { detached: false, stdio: ['ignore', 'pipe', 'pipe'], cwd: BUILD_DIR, env: cartEnv(cfg) })
      proc.stdout.on('data', chunk => send('cart:log', chunk.toString()))   // raylib trace (warnings only) + stray printf + net: HOSTING line
      proc.stderr.on('data', chunk => send('cart:log', chunk.toString()))   // printh() output
      proc.on('exit', (code, signal) => send('cart:exit', { code, signal }))
      proc.on('error', () => {})

      // also cross-compile for Windows in the background (console build for testing)
      if (mingwAvailable()) {
        exec(`${MINGW} ${winCompileArgs(dims, CART_EXE).join(' ')}`, () => {})
      }

      resolve({
        ok:     true,
        cmd,
        output: warnings || null,
        src:    CART_SRC,
        bin:    CART_BIN,
        exe:    CART_EXE,
        netIp:  cfg?.net?.mode === 'host' ? lanIPv4() : undefined,   // bare IP — that's what the joiner types
      })
    })
  })
})

// EXPORT A WINDOWS .exe you can send to someone (Thread 2 — the send-to-son path).
// Cross-compiles the current cart with -mwindows (no console window) and
// -DDE_NET_LOBBY_DEFAULT (double-click boots into the Host/Join/Solo lobby), asks
// where to save, and reveals it in Finder. Self-contained: sprites/font are baked
// into the binary. Windows will warn 'unknown publisher' for an unsigned exe —
// the recipient clicks 'More info → Run anyway' (code-signing is a later step).
ipcMain.handle('studio:export-win', async (_event, code, cfg) => {
  const wc  = _event.sender
  const log = (m) => { if (!wc.isDestroyed()) wc.send('cart:log', m) }
  if (!mingwAvailable()) return { ok: false, output: 'MinGW not found — install with: brew install mingw-w64' }
  if (!fs.existsSync(RAYLIB_WIN)) return { ok: false, output: `Windows raylib not found at ${RAYLIB_WIN} — brew install raylib-win64 (or adjust RAYLIB_WIN)` }

  const dims = prepareCart(code, cfg)   // writes cart.c + the embedded data headers
  const slug = (cfg?.cartName || 'cart').toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-+|-+$/g, '') || 'cart'
  const { canceled, filePath } = await dialog.showSaveDialog({
    title: 'Export Windows .exe',
    defaultPath: `${slug}.exe`,
    filters: [{ name: 'Windows executable', extensions: ['exe'] }],
  })
  if (canceled || !filePath) return { ok: false, output: 'export cancelled' }
  const out = filePath.endsWith('.exe') ? filePath : `${filePath}.exe`

  log(`\n── exporting ${path.basename(out)} for Windows (multiplayer lobby, no console) ──\n`)
  return new Promise(resolve => {
    exec(`${MINGW} ${winCompileArgs(dims, out, { lobby: true, gui: true, title: cfg?.cartName || slug }).join(' ')}`, (err, _o, stderr) => {
      if (err) { log((stderr || 'mingw build failed') + '\n'); return resolve({ ok: false, output: stderr || 'mingw build failed' }) }
      try { shell.showItemInFolder(out) } catch {}
      log(`✓ exported ${out}\n  send it over — on Windows it double-clicks into the multiplayer lobby (Host / Join / Solo).\n`)
      resolve({ ok: true, out })
    })
  })
})

// EXPORT A MAC APP you can send to another Mac. The native binary is self-contained
// (sprites/font baked in) and built with -DDE_NET_LOBBY_DEFAULT (boots into the
// Host/Join/Solo lobby). With a "Developer ID Application" cert in the keychain this
// hands off to tools/mac-app.sh — .app bundle → codesign → notarize (if the notary
// keychain profile exists) → staple — and saves a send-ready zip. Without the cert it
// falls back to the old bare unsigned binary. One-time cert/notary setup: the
// mac-app.sh header. macOS host only.
function macDevIdCert() {
  try {
    const m = execSync('security find-identity -v', { encoding: 'utf8' })
      .match(/"(Developer ID Application:[^"]*)"/)
    return m ? m[1] : ''
  } catch { return '' }
}
function macNotaryProfileStored() {
  // notarytool keeps creds in the data-protection keychain, invisible to `security
  // find-generic-password`. `history` exits 0 iff the profile exists and Apple is
  // reachable (~1s); a missing profile fails instantly (exit 69), offline fails too
  // — either way we degrade to sign-only, which the export log explains.
  try { execSync('xcrun notarytool history --keychain-profile dreamengine-notary', { stdio: 'ignore', timeout: 15000 }); return true }
  catch { return false }
}

ipcMain.handle('studio:export-mac', async (_event, code, cfg) => {
  const wc  = _event.sender
  const log = (m) => { if (!wc.isDestroyed()) wc.send('cart:log', m) }
  if (process.platform !== 'darwin') return { ok: false, output: 'Mac export runs on macOS only' }

  const dims  = prepareCart(code, cfg)
  const slug  = (cfg?.cartName || 'cart').toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-+|-+$/g, '') || 'cart'
  const devId = macDevIdCert()

  // ── no Developer ID cert → old flow: bare unsigned binary ────────────────
  if (!devId) {
    const { canceled, filePath } = await dialog.showSaveDialog({
      title: 'Export Mac binary (unsigned)',
      defaultPath: slug,   // no extension — a plain runnable Mach-O
    })
    if (canceled || !filePath) return { ok: false, output: 'export cancelled' }

    const args = macCompileArgs(dims, ['-O2', '-DDE_RELEASE'], filePath, ['-DDE_NET_LOBBY_DEFAULT', ...titleDef(cfg?.cartName || slug)])
    log(`\n── exporting ${path.basename(filePath)} for macOS (multiplayer lobby, UNSIGNED) ──\n`)
    log(`no "Developer ID Application" certificate in the keychain — exporting a bare binary.\n` +
        `for a signed, notarized .app that opens anywhere, do the one-time setup in tools/mac-app.sh's header.\n\n`)
    return new Promise(resolve => {
      exec(`clang ${args.join(' ')}`, (err, _o, stderr) => {
        const warn = stderr.split('\n').filter(l => !l.includes('was built for newer macOS version')).join('\n').trim()
        if (err) { log((warn || 'clang build failed') + '\n'); return resolve({ ok: false, output: warn || 'clang build failed' }) }
        try { fs.chmodSync(filePath, 0o755) } catch {}
        try { shell.showItemInFolder(filePath) } catch {}
        log(`✓ exported ${filePath}\n  self-contained — send just this file. On another Mac: right-click → Open (unsigned, so Gatekeeper asks once).\n`)
        resolve({ ok: true, out: filePath, msg: '✓ Mac binary exported (unsigned) — revealed in Finder' })
      })
    })
  }

  // ── Developer ID cert found → signed .app via tools/mac-app.sh, saved as a zip ──
  const pretty   = cfg?.cartName || slug
  const notarize = macNotaryProfileStored()
  const { canceled, filePath } = await dialog.showSaveDialog({
    title: notarize ? 'Export signed + notarized Mac app' : 'Export signed Mac app',
    defaultPath: `${slug}.zip`,   // zip keeps the .app bundle intact in transit
    filters: [{ name: 'Zipped Mac app', extensions: ['zip'] }],
  })
  if (canceled || !filePath) return { ok: false, output: 'export cancelled' }
  const zipOut = filePath.endsWith('.zip') ? filePath : `${filePath}.zip`

  const tmpBin = path.join(BUILD_DIR, `export-${slug}`)
  const args   = macCompileArgs(dims, ['-O2', '-DDE_RELEASE'], tmpBin, ['-DDE_NET_LOBBY_DEFAULT', ...titleDef(pretty)])
  log(`\n── exporting ${pretty}.app for macOS (multiplayer lobby, signed${notarize ? ' + notarized' : ''}) ──\n`)
  if (!notarize) log(`note: no notarytool keychain profile found — signing only (Gatekeeper still asks once\n` +
                     `on other Macs). Store creds per tools/mac-app.sh's header for the full flow.\n\n`)

  return new Promise(resolve => {
    exec(`clang ${args.join(' ')}`, (err, _o, stderr) => {
      const warn = stderr.split('\n').filter(l => !l.includes('was built for newer macOS version')).join('\n').trim()
      if (err) { log((warn || 'clang build failed') + '\n'); return resolve({ ok: false, output: warn || 'clang build failed' }) }

      const script = path.join(__dirname, '../../tools/mac-app.sh')
      const shArgs = [script, tmpBin, '--name', pretty, '--out', BUILD_DIR]
      if (!notarize) shArgs.push('--no-notarize')
      else log(`(notarization uploads to Apple — allow 1–5 min)\n`)

      const child = spawn('bash', shArgs, { env: { ...process.env, DEV_ID: devId } })
      child.stdout.on('data', d => log(d.toString()))
      child.stderr.on('data', d => log(d.toString()))
      child.on('close', codeNum => {
        try { fs.unlinkSync(tmpBin) } catch {}
        if (codeNum !== 0) return resolve({ ok: false, output: `mac-app.sh failed (exit ${codeNum}) — see the log above` })

        const app = path.join(BUILD_DIR, `${pretty}.app`)
        exec(`/usr/bin/ditto -c -k --keepParent "${app}" "${zipOut}"`, (zerr) => {
          if (zerr) return resolve({ ok: false, output: `zip failed: ${zerr.message}` })
          try { shell.showItemInFolder(zipOut) } catch {}
          log(`\n✓ exported ${zipOut}\n` + (notarize
            ? `  signed + notarized — on another Mac: unzip, double-click. No Gatekeeper warning.\n`
            : `  signed (not notarized) — on another Mac: unzip, right-click → Open once.\n`))
          resolve({ ok: true, out: zipOut, msg: notarize
            ? '✓ signed + notarized .app exported — revealed in Finder'
            : '✓ signed .app exported (not notarized) — revealed in Finder' })
        })
      })
    })
  })
})

// DEPLOY TO iPhone: build+sign the LIVE editor buffer for a connected device and launch it,
// streaming ios/device.sh's progress to the runtime log. prepareCart() already wrote
// build/{cart.c,sprites_data.h,map_data.h}; EDITOR=1 tells device.sh to use those as the app cart
// (not re-stage a saved one), and the cart's dims ride in via DE_* env (works for any size). The
// signed device build is ~90s — this is a deploy button, not a hot-run. Electron + a plugged-in,
// unlocked iPhone with signing set up (see ios/device.sh) required.
ipcMain.handle('studio:deploy-ios', async (_event, code, cfg) => {
  const dims    = prepareCart(code, cfg)
  const iosDir  = path.join(__dirname, '../../ios')
  const wc      = _event.sender
  const log     = (m) => { if (!wc.isDestroyed()) wc.send('cart:log', m) }
  if (!fs.existsSync(path.join(iosDir, 'device.sh'))) return { ok: false, output: 'ios/device.sh not found' }

  const iosConfig = cfg?.iosConfig === 'release' ? 'Release' : 'Debug'
  log(`── deploy to iPhone ──\nbuilding signed for device (${iosConfig}, ~90s)…\n`)
  const env = {
    ...process.env,
    EDITOR: '1',
    CONFIG:      iosConfig,
    CART:        cfg?.cartName || 'editor',
    DE_SCREEN_W: String(dims.screenW), DE_SCREEN_H: String(dims.screenH),
    DE_MAP_W:    String(dims.mapW),    DE_MAP_H:    String(dims.mapH),
    DE_CELL_W:   String(dims.cellW),   DE_CELL_H:   String(dims.cellH),
  }
  return new Promise(resolve => {
    const proc = spawn('bash', ['device.sh'], { cwd: iosDir, env, stdio: ['ignore', 'pipe', 'pipe'] })
    let tail = ''
    // The device build spews known-benign noise the maker doesn't need: xcodebuild's
    // "DVTAssertions … deviceType … NULL … file a bug" device-probe block, lldb's prep-command
    // dump, and ~50 near-identical "[NN%] Copying … to device" lines. Filter to the signal
    // (device.sh's ▸ steps, install milestones, ✓/errors). `tail` keeps the RAW output so a
    // real failure still surfaces its full error.
    const NOISE = /DVTAssertions|^\s*Details:|^\s*Object:|^\s*Method:|^\s*Thread:|feedbackassistant\.apple\.com|^\(lldb\)|^\s*command (script|source)|^Executing commands|^\s*Platform:\s|^\s*Connected:|^\s*Sysroot:|^\s*SDK Path:|^\s*Symbol Path:|target modules search-paths|^Current executable set|developer disk image/i
    let lineBuf = '', copyShown = false, lastAt = Date.now()
    const emit = (ln) => {
      if (NOISE.test(ln)) return
      if (/^\[\s*\d+%\]\s+Copying .* to device\s*$/.test(ln)) {   // collapse the copy spam to one line
        if (copyShown) return
        copyShown = true; log('[..] copying app to device…\n'); return
      }
      log(ln + '\n')
    }
    const cap = (c) => {
      const s = c.toString(); tail = (tail + s).slice(-400); lastAt = Date.now()
      lineBuf += s
      const lines = lineBuf.split('\n'); lineBuf = lines.pop()
      for (const ln of lines) emit(ln)
    }
    // heartbeat: the signed xcodebuild is silent for ~90s — emit a dot during idle stretches so
    // it's visibly working, not hung.
    const hb = setInterval(() => { if (Date.now() - lastAt > 5000) { lastAt = Date.now(); log('·') } }, 5000)
    proc.stdout.on('data', cap)
    proc.stderr.on('data', cap)
    proc.on('exit', (codeN) => {
      clearInterval(hb)
      if (lineBuf) emit(lineBuf)   // flush any trailing partial line
      resolve(codeN === 0
        ? { ok: true,  output: null }
        : { ok: false, output: tail.trim() || `device.sh exited ${codeN}` })
    })
    proc.on('error', (e) => { clearInterval(hb); resolve({ ok: false, output: String(e) }) })
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
      const cartTitle = cfg?.cartName ? `${cfg.cartName} (profiling)` : 'dreamengine (profiling)'

      // clear any stale perf.json so a crash-before-first-flush can't show old data
      try { fs.unlinkSync(path.join(BUILD_DIR, 'perf.json')) } catch {}

      const proc = spawn(CART_BIN, ['--title', cartTitle, ...saveDirArgs(cfg)], { detached: false, stdio: ['ignore', 'pipe', 'pipe'], cwd: BUILD_DIR, env: cartEnv(cfg) })
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

// first non-internal IPv4 — so a phone/iPad on the same wifi can open the cart
function lanAddress() {
  for (const ifaces of Object.values(os.networkInterfaces()))
    for (const i of ifaces)
      if (i.family === 'IPv4' && !i.internal) return i.address
  return null
}

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
    // 0.0.0.0 so devices on the LAN (iPad multitouch testing) can reach it too
    webServer.listen(WEB_PORT, '0.0.0.0', () => resolve(url))
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
    `-DSCALE_FILTER=${cfg?.scaleFilter || 0}`,   // present filter; modes 2/3 are desktop-only, web falls back
    `-DRENDER_EVERY=${cfg?.renderEvery ?? 1}`,   // present every Nth tick (web heat lever); 1 = every tick
    '-Os',
    '-fno-delete-null-pointer-checks',
    `"${RAYLIB_WEB}/lib/libraylib.a"`,
    '-s USE_GLFW=3',
    '-s TOTAL_MEMORY=67108864',
    '-s EXPORTED_RUNTIME_METHODS=ccall,HEAPF32',
    `--post-js "${path.join(RUNTIME_DIR, 'web_midi.js')}"`,   // Web MIDI → midi_get() bridge
  ]   // baseArgs — the shell/output tail is appended per build below

  // AudioWorklet backend (settings → "audio (web)"): mirror build-site — ship BOTH a
  // worklet build and a ScriptProcessor fallback + the auto-pick loader + coi. Default
  // OFF, so the normal preview path is unchanged. See design/audio-threading.md.
  const wantWorklet = !!cfg?.worklet
  const workletFlags = [
    '-DDE_AUDIO_WORKLET', '-s AUDIO_WORKLET=1', '-s WASM_WORKERS=1',
    `--js-library "${path.join(RUNTIME_DIR, 'audio-worklet-stub.js')}"`,
  ].join(' ')
  const runEmcc = (extra) => new Promise((res, rej) =>
    exec(`emcc ${args.join(' ')} ${extra}`, { timeout: 120000 },
      (e, _o, se) => e ? rej(new Error((se || '').trim() || e.message)) : res()))

  log(wantWorklet ? 'running emcc (worklet + fallback)…\n' : 'running emcc… (this takes ~10s)\n')
  try {
    if (wantWorklet) {
      await runEmcc(`-o "${path.join(BUILD_DIR, 'plain.js')}"`)                         // ScriptProcessor fallback
      await runEmcc(`${workletFlags} -o "${path.join(BUILD_DIR, 'worklet.js')}"`)        // AudioWorklet (shared memory)
      fs.copyFileSync(path.join(RUNTIME_DIR, 'web_shell_worklet.html'), CART_HTML)       // loader + coi + self-heal
      fs.copyFileSync(path.join(RUNTIME_DIR, 'coi-serviceworker.js'), path.join(BUILD_DIR, 'coi-serviceworker.js'))
      for (const f of ['index.js', 'index.wasm']) { const p = path.join(BUILD_DIR, f); if (fs.existsSync(p)) fs.unlinkSync(p) }
    } else {
      await runEmcc(`--shell-file "${shellHtml}" -o "${CART_HTML}"`)
    }
  } catch (e) {
    log(String(e.message).trim())
    return { ok: false, output: String(e.message).trim() }
  }
  const url = await startWebServer()
  shell.openExternal(url)
  log(`✓ done — opening ${url}${wantWorklet ? '  (worklet build — activates on a cross-origin-isolated host, else falls back)' : ''}\n`)
  const lan = lanAddress()
  if (lan) log(`  on your iPad/phone (same wifi): http://${lan}:${WEB_PORT}/cart.html\n`)
  return { ok: true, url, output: null }
})

// open a URL in the system browser (rlog's clickable links)
ipcMain.handle('studio:open-external', (_e, url) => {
  if (typeof url === 'string' && /^https?:\/\//.test(url)) shell.openExternal(url)
})
// open a LOCAL file with its default app — only within the repo (press kits, exports)
ipcMain.handle('studio:open-path', (_e, p) => {
  const abs = String(p || ''), ROOT = path.join(__dirname, '../..')
  if (abs && abs.startsWith(ROOT)) shell.openPath(abs)
})

// ── Apps view: list app manifests (apps/<name>/app.json) ──────
ipcMain.handle('studio:list-apps', async () => {
  const APPS_DIR = path.join(__dirname, '../../apps')
  try {
    const apps = []
    for (const dir of fs.readdirSync(APPS_DIR)) {
      const mf = path.join(APPS_DIR, dir, 'app.json')
      if (!fs.existsSync(mf)) continue
      try {
        const m = JSON.parse(fs.readFileSync(mf, 'utf8'))
        const iap = (m.iap && Array.isArray(m.iap.products)) ? m.iap.products.length : 0
        apps.push({ dir, name: m.name || dir, carts: m.carts || [], launcher: m.launcher || '', iap, listing: m.listing || null })
      } catch {}
    }
    return { ok: true, apps }
  } catch (e) { return { ok: false, apps: [], output: String(e.message || e) } }
})

// ── ASO toolkit (standalone — no app needed) ──────────────────
// Runs a tools/aso-*.js and streams its output to a dedicated 'aso:log' channel
// (not cart:log — keep it out of the runtime log). App-agnostic: research/lint/compose
// any input without selecting or altering an app.
function runAsoTool(_event, script, args, channel = 'aso:log') {
  const wc = _event.sender
  const log = (m) => { if (!wc.isDestroyed()) wc.send(channel, m) }
  const ROOT = path.join(__dirname, '../..')
  return new Promise(resolve => {
    const proc = spawn('node', [path.join(ROOT, 'tools', script), ...args], { cwd: ROOT })
    proc.stdout.on('data', c => log(c.toString()))
    proc.stderr.on('data', c => log(c.toString()))
    proc.on('exit', code => resolve({ ok: code === 0 }))
    proc.on('error', e => { log(String(e.message) + '\n'); resolve({ ok: false }) })
  })
}
const flag = (k, v) => (v && String(v).trim() ? ['--' + k, String(v)] : [])
ipcMain.handle('studio:aso-research', async (_e, terms, country) => {
  const list = String(terms || '').split(',').map(s => s.trim()).filter(Boolean)
  if (!list.length) return { ok: false, error: 'type at least one search term' }
  const cc = /^[a-z]{2}$/i.test(country || '') ? String(country).toLowerCase() : 'us'
  const ROOT = path.join(__dirname, '../..')
  // --json → collect + parse, so the editor can render clickable results (App Store links)
  return new Promise(resolve => {
    let out = '', err = ''
    const proc = spawn('node', [path.join(ROOT, 'tools/aso-research.js'), '--json', '--country', cc, ...list], { cwd: ROOT })
    proc.stdout.on('data', c => out += c.toString())
    proc.stderr.on('data', c => err += c.toString())
    proc.on('exit', code => {
      if (code !== 0) return resolve({ ok: false, error: err.trim() || 'research failed' })
      try { resolve({ ok: true, data: JSON.parse(out) }) }
      catch { resolve({ ok: false, error: 'could not parse results' }) }
    })
    proc.on('error', e => resolve({ ok: false, error: String(e.message) }))
  })
})
ipcMain.handle('studio:aso-lint', async (_e, f = {}) =>
  runAsoTool(_e, 'aso-lint.js', [...flag('title', f.title), ...flag('subtitle', f.subtitle),
    ...flag('keywords', f.keywords), ...flag('research', f.research)]))
ipcMain.handle('studio:aso-compose', async (_e, f = {}) =>
  runAsoTool(_e, 'aso-compose.js', [...flag('title', f.title), ...flag('subtitle', f.subtitle),
    ...flag('candidates', f.candidates)]))

// ── Apps view: per-app actions (app-scoped) ───────────────────
// build the multi-cart app (Mac / iOS) or its press page, streaming to the runtime log.
function spawnP(bin, args, cwd, log) {
  return new Promise(resolve => {
    const p = spawn(bin, args, { cwd })
    if (log) { p.stdout.on('data', c => log(c.toString())); p.stderr.on('data', c => log(c.toString())) }
    p.on('exit', code => resolve(code === 0))
    p.on('error', e => { if (log) log(String(e.message) + '\n'); resolve(false) })
  })
}
// generate per-app screenshots into apps/<name>/screenshots (dump each cart → store-shots).
// press-kit reads that dir — so this is what gives an app real, correct (never cross-app) shots.
ipcMain.handle('studio:app-shots', async (_e, name) => {
  const wc = _e.sender
  const log = (m) => { if (!wc.isDestroyed()) wc.send('cart:log', m) }
  if (!/^[a-z0-9_-]+$/i.test(name || '')) return { ok: false, output: 'bad app name' }
  const ROOT = path.join(__dirname, '../..')
  let carts = []
  try { carts = JSON.parse(fs.readFileSync(path.join(ROOT, 'apps', name, 'app.json'), 'utf8')).carts || [] }
  catch { return { ok: false, output: 'no manifest' } }
  if (!carts.length) { log('\nno carts in manifest\n'); return { ok: false } }
  const shotsDir = path.join(ROOT, 'apps', name, 'screenshots')
  fs.mkdirSync(shotsDir, { recursive: true })
  const scratch = path.join(ROOT, 'build', '.appshots')
  log(`\n── screenshots for ${name}: ${carts.join(', ')} ──\n`)
  for (const cart of carts) {
    if (!/^[a-z0-9_-]+$/i.test(cart)) continue
    const dump = path.join(scratch, cart)
    log(`\n· ${cart}: dumping frames…\n`)
    if (!await spawnP('node', [path.join(ROOT, 'tools/play.js'), cart, 'run', '--headless', '--frames', '40', '--dump', dump], ROOT, log)) {
      log(`  (dump failed — skipped)\n`); continue
    }
    let frame = ''
    try { const fr = fs.readdirSync(dump).filter(f => /\.png$/.test(f)).sort(); frame = fr[Math.floor(fr.length / 2)] } catch {}
    if (!frame) { log(`  (no frames — skipped)\n`); continue }
    const named = path.join(scratch, `${cart}.png`)             // unique name → no cross-cart overwrite
    fs.copyFileSync(path.join(dump, frame), named)
    log(`· ${cart}: composing shot…\n`)
    await spawnP('node', [path.join(ROOT, 'tools/store-shots.js'), '--in', named, '--device', 'iphone69', '--out', shotsDir], ROOT, log)
  }
  log(`\n✓ screenshots → apps/${name}/screenshots — run 📄 press kit to include them\n`)
  return { ok: true }
})
ipcMain.handle('studio:build-app', async (_e, name, target) => {
  const wc = _e.sender
  const log = (m) => { if (!wc.isDestroyed()) wc.send('cart:log', m) }
  if (!/^[a-z0-9_-]+$/i.test(name || '')) return { ok: false, output: 'bad app name' }
  const ROOT = path.join(__dirname, '../..')
  const args = [path.join(ROOT, 'tools/build-app.js'), name]
  if (target === 'mac') args.push('--mac'); else if (target === 'ios') args.push('--ios')
  log(`\n── build-app ${name}${target ? ' --' + target : ''} ──\n`)
  return new Promise(resolve => {
    const proc = spawn('node', args, { cwd: ROOT })
    proc.stdout.on('data', c => log(c.toString()))
    proc.stderr.on('data', c => log(c.toString()))
    proc.on('exit', code => resolve({ ok: code === 0 }))
    proc.on('error', e => { log(String(e.message) + '\n'); resolve({ ok: false }) })
  })
})
// lint / compose the app's OWN listing (from app.json's "listing" block) → runtime log
ipcMain.handle('studio:aso-app', async (_e, name, tool) => {
  const wc = _e.sender
  const log = (m) => { if (!wc.isDestroyed()) wc.send('cart:log', m) }
  if (!/^[a-z0-9_-]+$/i.test(name || '')) return { ok: false, output: 'bad app name' }
  const ROOT = path.join(__dirname, '../..')
  let L = {}
  try { L = JSON.parse(fs.readFileSync(path.join(ROOT, 'apps', name, 'app.json'), 'utf8')).listing || {} }
  catch { return { ok: false, output: 'no manifest' } }
  if (!L.title && !L.subtitle && !L.keywords) {
    log('\nno "listing" block in app.json — add title / subtitle / keywords\n'); return { ok: false }
  }
  const compose = tool === 'compose'
  log(`\n── aso-${compose ? 'compose' : 'lint'} ${name} (from app.json listing) ──\n`)
  const args = compose
    ? [...flag('title', L.title), ...flag('subtitle', L.subtitle), ...flag('candidates', L.keywords)]
    : [...flag('title', L.title), ...flag('subtitle', L.subtitle), ...flag('keywords', L.keywords)]
  return runAsoTool(_e, compose ? 'aso-compose.js' : 'aso-lint.js', args, 'cart:log')
})
ipcMain.handle('studio:press-kit', async (_e, name) => {
  const wc = _e.sender
  const log = (m) => { if (!wc.isDestroyed()) wc.send('cart:log', m) }
  if (!/^[a-z0-9_-]+$/i.test(name || '')) return { ok: false, output: 'bad app name' }
  const ROOT = path.join(__dirname, '../..')
  log(`\n── press-kit ${name} ──\n`)
  // per-app screenshots dir — NOT the shared build/.shots (that pulls another app's frames).
  // Empty until the per-app store-shots pipeline populates it → a sparse-but-correct kit.
  const shotsDir = path.join('apps', name, 'screenshots')
  return new Promise(resolve => {
    const proc = spawn('node', [path.join(ROOT, 'tools/press-kit.js'), name, '--shots', shotsDir], { cwd: ROOT })
    proc.stdout.on('data', c => log(c.toString()))
    proc.stderr.on('data', c => log(c.toString()))
    proc.on('exit', code => {
      const out = path.join(ROOT, 'site/press', name, 'index.html')
      if (code === 0 && fs.existsSync(out)) log(`\npreview: file://${out}\n`)   // clickable in the log
      resolve({ ok: code === 0 })
    })
    proc.on('error', e => { log(String(e.message) + '\n'); resolve({ ok: false }) })
  })
})

// ── publish to site ───────────────────────────────────────────
// Builds the CURRENT editor cart (code + sprites + map + settings) straight to
// site/<name>/, writes the C source back to tools/carts/<name>.c (repo and site
// stay in sync), then commits site/ + the write-back and pushes — live at
// https://nikkikoole.github.io/dreamengine/<name>/ ~1 min later. Gated behind
// the settings → "publish to site" toggle; renderer confirms before invoking.
// Post-processing (manifest/title/thumbnail/gallery) lives in build-site.js
// --finish; the git step (with the staged-strays guard) in publish-cart.sh.
ipcMain.handle('studio:publish', async (_event, code, cfg) => {
  const ROOT_DIR = path.join(__dirname, '../..')
  const wc  = _event.sender
  const log = (msg) => { if (!wc.isDestroyed()) wc.send('cart:log', msg) }

  const name = String(cfg?.cartName || '').toLowerCase().replace(/\.cart\.png$/, '').replace(/[^a-z0-9_-]+/g, '')
  if (!name) return { ok: false, output: 'the cart needs a name — load a cart, or "save cart" first' }

  const RAYLIB_WEB = path.join(RUNTIME_DIR, 'raylib-web')
  if (!fs.existsSync(path.join(RAYLIB_WEB, 'include', 'raylib.h')))
    return { ok: false, output: 'raylib-web missing (see "Build for web" for setup)' }

  log(`── publish "${name}" ──\n`)
  fs.mkdirSync(BUILD_DIR, { recursive: true })
  fs.writeFileSync(CART_SRC, code)

  // asset headers from the just-saved editor sprites/map (same as build-web)
  const mkHeader = (file, sym) => {
    const out = path.join(BUILD_DIR, `${sym.toLowerCase()}.h`)
    if (fs.existsSync(path.join(BUILD_DIR, file))) {
      try {
        const xxd = execSync(`xxd -i ${file}`, { cwd: BUILD_DIR }).toString()
        fs.writeFileSync(out, xxd
          .replace(new RegExp(`unsigned char ${file.replace('.', '_')}\\[\\]`), `static const unsigned char ${sym}[]`)
          .replace(new RegExp(`unsigned int ${file.replace('.', '_')}_len`),  `static const unsigned int  ${sym}_LEN`))
        return
      } catch {}
    }
    fs.writeFileSync(out, `static const unsigned char ${sym}[]={0};static const unsigned int ${sym}_LEN=0;\n`)
  }
  mkHeader('sprites.png', 'SPRITES_DATA')
  mkHeader('map.dat', 'MAP_DATA')

  const outDir = path.join(ROOT_DIR, 'site', name)
  fs.mkdirSync(outDir, { recursive: true })
  const args = [
    CART_SRC, path.join(RUNTIME_DIR, 'studio.c'),
    '-I', RUNTIME_DIR, '-I', BUILD_DIR, '-I', path.join(RAYLIB_WEB, 'include'),
    '-DPLATFORM_WEB',
    `-DSCREEN_W=${cfg?.screenW || 320}`, `-DSCREEN_H=${cfg?.screenH || 200}`, `-DSCALE=${cfg?.scale || 4}`,
    `-DMAP_W=${cfg?.mapW || 128}`, `-DMAP_H=${cfg?.mapH || 64}`,
    `-DCELL_W=${cfg?.cellW || 16}`, `-DCELL_H=${cfg?.cellH || 16}`,
    `-DTOUCH_CONTROLS_DEFAULT=${cfg?.touchControls ? 1 : 0}`,
    '-Os', '-fno-delete-null-pointer-checks',
    path.join(RAYLIB_WEB, 'lib', 'libraylib.a'),
    '-s', 'USE_GLFW=3', '-s', 'TOTAL_MEMORY=67108864',
    '-s', 'EXPORTED_RUNTIME_METHODS=ccall,HEAPF32',
  ]   // baseArgs — shell/output tail appended per build below

  // AudioWorklet backend: MATCH build-site — instrument-kind carts (looked up in
  // index.json by name) or worklet:true ship a worklet build + a ScriptProcessor
  // fallback + the auto-pick loader + coi; everything else builds plain. worklet:false
  // opts out. See design/audio-threading.md.
  let kindWorklet = false
  try {
    const idx = JSON.parse(fs.readFileSync(path.join(ROOT_DIR, 'editor', 'public', 'carts', 'index.json'), 'utf8'))
    const e = (Array.isArray(idx) ? idx : idx.carts || []).find(c => c.file === `${name}.cart.png`)
    kindWorklet = !!e && (e.kind || []).includes('instrument')
  } catch {}
  const wantWorklet = cfg?.worklet === true || (cfg?.worklet !== false && kindWorklet)
  const workletFlags = ['-DDE_AUDIO_WORKLET', '-s', 'AUDIO_WORKLET=1', '-s', 'WASM_WORKERS=1',
                        '--js-library', path.join(RUNTIME_DIR, 'audio-worklet-stub.js')]

  log(`compiling for web…${wantWorklet ? ' (worklet + fallback)' : ' (~10s)'}\n`)
  const step = (file, fargs, opts = {}) => new Promise((res, rej) => {
    const { execFile } = require('child_process')
    execFile(file, fargs, { timeout: 180000, cwd: ROOT_DIR, ...opts }, (err, stdout, stderr) =>
      err ? rej(new Error((stderr || stdout || err.message).trim())) : res(stdout))
  })

  try {
    if (wantWorklet) {
      await step('emcc', [...args, '-o', path.join(outDir, 'plain.js')])                  // ScriptProcessor fallback
      await step('emcc', [...args, ...workletFlags, '-o', path.join(outDir, 'worklet.js')]) // AudioWorklet
      fs.copyFileSync(path.join(RUNTIME_DIR, 'web_shell_worklet.html'), path.join(outDir, 'index.html'))
      fs.copyFileSync(path.join(RUNTIME_DIR, 'coi-serviceworker.js'), path.join(outDir, 'coi-serviceworker.js'))
      for (const f of ['index.js', 'index.wasm']) { const p = path.join(outDir, f); if (fs.existsSync(p)) fs.unlinkSync(p) }
    } else {
      await step('emcc', [...args, '--shell-file', path.join(RUNTIME_DIR, 'web_shell.html'), '-o', path.join(outDir, 'index.html')])
    }
    for (const f of (wantWorklet ? ['worklet.wasm', 'plain.wasm'] : ['index.html', 'index.js', 'index.wasm'])) {
      const fp = path.join(outDir, f)
      if (fs.existsSync(fp)) log(`  ${fp}  (${Math.round(fs.statSync(fp).size / 1024)} KB)\n`)
    }

    // write the source back so tools/carts/ and the site can't drift.
    // (sprites are NOT written back: editor pixels and a .cart.js generator are
    // two different sources of truth — see STATUS "sprite story" open item)
    const cartC = path.join(ROOT_DIR, 'tools', 'carts', `${name}.c`)
    fs.writeFileSync(cartC, code)
    log(`✓ source written back to tools/carts/${name}.c\n`)
    if (fs.existsSync(cartC.replace(/\.c$/, '.cart.js')))
      log(`⚠ ${name}.cart.js exists — editor-drawn sprite/map changes are in THIS build,\n  but the .cart.js generator was not updated (the sprite story, see STATUS)\n`)

    log('finishing (manifest, thumbnail, gallery)…\n')
    await step('node', ['tools/build-site.js', '--finish', name])

    log('committing + pushing…\n')
    const url = `https://nikkikoole.github.io/dreamengine/${name}/`
    const out = await step('sh', ['tools/publish-cart.sh', '--no-build', name])

    // unchanged cart → identical wasm → nothing committed or pushed. Say so
    // and skip the deploy watcher (there will be no deploy).
    if (out.includes('nothing new to publish')) {
      log(`✓ nothing changed since the last publish — no push needed.\n`)
      log(`▶ the current version is already live: ${url}\n`)
      return { ok: true, url, output: null }
    }

    // the script echoes its own URLs — drop those lines so the live link below
    // is the ONE clickable url in the log
    log(out.split('\n').filter(l => l.trim() && !l.includes('https://')).join('\n') + '\n')

    log(`✓ the wasm lives in the dreamengine repo under site/${name}/ — pushed.\n`)
    log(`⏳ waiting for GitHub Pages to deploy (~1 min)…\n`)
    log(`   follow it live: https://github.com/NikkiKoole/dreamengine/actions\n`)

    // watch OUR commit's deploy run via the public Actions API (no auth needed
    // for a public repo) and announce when the cart is actually playable
    const sha = execSync('git rev-parse HEAD', { cwd: ROOT_DIR }).toString().trim()
    ;(async () => {
      const api = `https://api.github.com/repos/NikkiKoole/dreamengine/actions/runs?head_sha=${sha}&per_page=1`
      let emptyPolls = 0
      for (let i = 0; i < 24; i++) {                       // ~4 min max
        await new Promise(r => setTimeout(r, 10000))
        try {
          const res = await fetch(api, { headers: { accept: 'application/vnd.github+json' } })
          if (!res.ok) continue
          const run = (await res.json()).workflow_runs?.[0]
          if (!run && ++emptyPolls >= 6) {                 // a minute with no run at all
            log(`⚠ no deploy run appeared for this push — check https://github.com/NikkiKoole/dreamengine/actions\n`)
            return
          }
          if (run?.status === 'completed') {
            log(run.conclusion === 'success'
              ? `🟢 deployed — it's live: ${url}\n`
              : `🔴 deploy ${run.conclusion} — see https://github.com/NikkiKoole/dreamengine/actions\n`)
            return
          }
        } catch {}
      }
      log(`⚠ couldn't confirm the deploy — check https://github.com/NikkiKoole/dreamengine/actions\n`)
    })()

    return { ok: true, url, output: null }
  } catch (e) {
    log('✗ publish failed:\n' + e.message + '\n')
    return { ok: false, output: e.message }
  }
})
