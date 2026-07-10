const { app, BrowserWindow, ipcMain, Menu, session, dialog, shell, nativeImage, globalShortcut } = require('electron')
const { exec, execSync, spawn, spawnSync } = require('child_process')
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

// Read the cart's de:meta "resizable" flag from the live source buffer (the same block regex
// build-cart-index.js/lint-carts.js use). Malformed/absent meta → false, never throws (the run
// path must not die on a mid-edit unparseable comment). See docs/design/cart-metadata.md.
function parseResizable(code) {
  try {
    const m = String(code || '').match(/\/\*\s*de:meta\s*\n([\s\S]*?)\nde:meta\s*\*\//)
    return m ? JSON.parse(m[1]).resizable === true : false
  } catch { return false }
}

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
    // device-adaptive-layout.md Phase 1b: a cart with "resizable": true in its de:meta compiles
    // with -DDE_RESIZABLE → a live-reflowing window (screen_w()/screen_h()). Parsed from the live
    // editor buffer with build-cart-index.js's canonical de:meta regex. Default off (fixed canvas).
    resizable: parseResizable(code),
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
  const { screenW, screenH, scale, mapW, mapH, cellW, cellH, touchDefault, scaleFilter, keymap, studioC, resizable } = dims
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
    ...(resizable ? ['-DDE_RESIZABLE'] : []),   // de:meta resizable → live-reflow window (studio.c owns FLAG_WINDOW_RESIZABLE)
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

// ── native cart process registry ───────────────────────────────────────────
// The native run/record/replay children used to be fire-and-forget (a throwaway
// `const proc`), so nothing could ever find them again. We keep a handle now,
// keyed by pid, so the Debug menu can attach a profiler/sampler to the cart you
// are actually PLAYING (not a cold respawn — STATUS #45) — and so future
// cross-cart actions have one place to look. We deliberately DON'T kill a
// previous cart on a new Run: several carts running at once is supported (the OS
// schedules + mixes them for free; play.js netdemo relies on it).
const nativeCarts = new Map()   // pid -> { proc, name, kind, startedAt, recPath, slug }
function trackNative(proc, name, kind, meta) {
  if (!proc || !proc.pid) return proc
  nativeCarts.set(proc.pid, { proc, name: name || 'dreamengine', kind, startedAt: Date.now(),
                              recPath: meta?.recPath || null, slug: meta?.slug || null })
  refreshDebugMenu(); updateCartShortcut()
  proc.on('exit', () => { nativeCarts.delete(proc.pid); refreshDebugMenu(); updateCartShortcut() })
  return proc
}
function runningCarts() {   // most-recently-started first
  return [...nativeCarts.values()].sort((a, b) => b.startedAt - a.startedAt)
}

// ── flight recorder: always-on deterministic session capture ────────────────
// Every native Run records to a per-cart RING (build/.rec/<slug>/, gitignored), kept cheap by
// delta-encoded input logging. We throw away almost all of it (keep the last RING_KEEP) and
// "keep a take" only when we care → promote into tools/clips/<slug>/. Because the run is --det
// with a logged seed, any moment replays exactly (bug repro / clip). Design: docs/design/flight-recorder.md
const RING_KEEP = 10
// canonical cart id (the .cart.png / tools/carts/<slug>.c / tools/clips/<slug> stem), NOT the
// display title — "Squishy Lines" must land in .../squishy/, not .../squishy-lines/.
function cartSlug(cfg) {
  return (cfg?.cartFile || cfg?.cartName || 'scratch').toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-+|-+$/g, '') || 'scratch'
}
function ringDir(slug) { return path.join(BUILD_DIR, '.rec', slug) }
function sessionStamp() { return `${Date.now().toString(36)}-${Math.floor(Math.random() * 1e6).toString(36)}` }
function pruneRing(slug) {                                    // keep the newest RING_KEEP, evict the rest
  try {
    const dir = ringDir(slug)
    const files = fs.readdirSync(dir).filter(f => f.endsWith('.rec'))
      .map(f => ({ f, t: fs.statSync(path.join(dir, f)).mtimeMs })).sort((a, b) => b.t - a.t)
    for (const e of files.slice(RING_KEEP)) { try { fs.unlinkSync(path.join(dir, e.f)) } catch {} }
  } catch {}
}
// promote a ring take into its committed home tools/clips/<slug>/NN-take.rec. Self-contained: the
// engine already wrote a `# seed` header, so the copy replays into the exact same world.
function keepTake(ringFile, slug, send) {
  const ROOT = path.join(__dirname, '../..')
  try {
    if (!ringFile || !fs.existsSync(ringFile) || fs.statSync(ringFile).size === 0)
      return (send && send('cart:log', '\n(no take to keep — nothing recorded this session yet)\n'), { ok: false })
    const dir = path.join(ROOT, 'tools/clips', slug)
    fs.mkdirSync(dir, { recursive: true })
    const nums = fs.readdirSync(dir).map(f => { const m = f.match(/^(\d+)-/); return m ? +m[1] : 0 })
    const nn = String((nums.length ? Math.max(...nums) : 0) + 1).padStart(2, '0')
    const dest = path.join(dir, `${nn}-take.rec`)
    fs.copyFileSync(ringFile, dest)
    const rel = path.relative(ROOT, dest)
    if (send) send('cart:log', `\n✓ kept take → ${rel}\n  replay:  node tools/play.js ${slug} replay ${rel}\n  clip:    node tools/make-gif.js ${slug} --recipe ${nn}-take\n`)
    return { ok: true, rel, dest, slug, label: `${nn}-take` }
  } catch (e) { if (send) send('cart:log', `\n✗ keep take failed: ${e.message}\n`); return { ok: false, error: String(e.message || e) } }
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

// Promisified exec + a poll for the live-inspection mailbox handshake (the request
// file disappearing = the running cart served it). Used by attach-profiling.
function execP(cmd) { return new Promise((res, rej) => exec(cmd, err => err ? rej(err) : res())) }
function waitForConsumed(file, ms) {
  return new Promise(resolve => {
    const start = Date.now()
    const tick = () => {
      if (!fs.existsSync(file)) return resolve(true)       // gone = served (fresh dump written)
      if (Date.now() - start > ms) return resolve(false)   // timed out (cart not polling / DE_RELEASE build)
      setTimeout(tick, 80)
    }
    tick()
  })
}

// Read the in-engine perf.json the profiling build (-DDE_PROFILE) writes: frame
// CPU timing (C) + per-primitive draw-call counts (D). Returns null if absent.
function readPerf() { return readPerfFrom(path.join(BUILD_DIR, 'perf.json')) }
function readPerfFrom(file) {
  try {
    const j = JSON.parse(fs.readFileSync(file, 'utf8'))
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

app.on('will-quit', () => { try { globalShortcut.unregisterAll() } catch {} })

// The app menu is rebuilt whenever the loaded cart changes, so the macOS menu
// bar shows what's open (" dreamengine  Edit  View  <cart>") — the window's own
// titlebar is hidden (hiddenInset), so this is the visible "what am I editing".
let _menuCartName = ''       // last cart name we were told about (survives Debug-menu rebuilds)
function buildAppMenu(cartName) {
  if (typeof cartName === 'string') _menuCartName = cartName
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
  if (_menuCartName) {
    // macOS top-level menus must have a submenu; this one is informational
    template.push({
      label: _menuCartName,
      submenu: [{ label: 'loaded cart', enabled: false }],
    })
  }
  // Debug menu — the one place for cross-cart dev actions. Always present in the
  // editor (it's unobtrusive up here); absent entirely from any deployed/exported
  // cart, which is the standalone C binary with no Electron menu. Lists each
  // RUNNING cart with its two actions: attach-profile (⌘⇧P) and keep-take (⌘⇧K,
  // promote the flight-recorder session to tools/clips). The hotkeys live on the
  // global shortcut (updateCartShortcut) so they fire while the CART window is
  // focused too — menu items are click-only to avoid a double-bind on the accel.
  {
    const carts = runningCarts()
    let items
    if (!carts.length) {
      items = [{ label: 'Run a cart first  (▶) — then profile / keep its take here', enabled: false }]
    } else {
      items = []
      carts.forEach((c, i) => {
        if (i) items.push({ type: 'separator' })
        items.push({ label: `Profile “${c.name}”  (attach, ⌘⇧P)`, click: () => profileAttachFromMenu(c.proc.pid) })
        items.push({ label: `Keep take of “${c.name}”  (⌘⇧K)`, enabled: !!c.recPath, click: () => keepTakeFromMenu(c.proc.pid) })
      })
    }
    template.push({ label: 'Debug', submenu: items })
  }
  Menu.setApplicationMenu(Menu.buildFromTemplate(template))
}
function refreshDebugMenu() { buildAppMenu(_menuCartName) }

// The ⌘⇧P (attach-profile) and ⌘⇧K (keep take) hotkeys are GLOBAL shortcuts (not just menu
// accelerators) so they work while the cart window — not the editor — is focused. Registered only
// while a cart is running, released otherwise, so they never permanently claim the combos.
let cartShortcutOn = false
function updateCartShortcut() {
  const want = nativeCarts.size > 0
  if (want && !cartShortcutOn) {
    try {
      globalShortcut.register('CommandOrControl+Shift+P', () => profileAttachFromMenu(null))
      globalShortcut.register('CommandOrControl+Shift+K', () => keepTakeFromMenu(null))
      cartShortcutOn = true
    } catch {}
  } else if (!want && cartShortcutOn) {
    try { globalShortcut.unregister('CommandOrControl+Shift+P'); globalShortcut.unregister('CommandOrControl+Shift+K') } catch {}
    cartShortcutOn = false
  }
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

// ── cart save-to-SOURCE (repo carts) ──────────────────────────
// A gallery/tutorial cart's real source of truth is tools/carts/<slug>.c (what
// make-cart.js bakes from); the .cart.png is a build product the editor otherwise
// only reaches via Save As. This writes the Code-tab buffer back to that .c and
// re-bakes the gallery .cart.png in place (keeping its baked thumbnail), so an
// edit survives the next CLI bake and is ready to commit. `slug` comes from the
// cart's de:meta (backfilled + lint-required) — the PNG→source anchor
// (editor-cart-workflow Gap 1/1b). Source only: sprites/map are NOT written to a
// .cart.js generator: hand-edits to a generator cart's sheet are persisted as a
// reversible slot-level PATCH (editor-cart-workflow Gap 2, "Option D") — the
// generator stays a live program, the touch-ups survive the next CLI bake. See
// tools/lib/sprite-patch.js. Deliberately NOT a git commit (cart-commit.js does that).
ipcMain.handle('cart:save-to-source', async (_event, { slug, source, spritesDataUrl, mapBase64, settings, spriteSlots }) => {
  if (!slug || !/^[\w-]+$/.test(slug)) return { ok: false, error: 'no cart slug (open a repo cart first)' }
  const ROOT    = path.join(__dirname, '../..')
  const cPath   = path.join(ROOT, 'tools', 'carts', `${slug}.c`)
  const pngPath = path.join(GALLERY_DIR, `${slug}.cart.png`)
  if (!fs.existsSync(cPath))   return { ok: false, error: `no tools/carts/${slug}.c — not a repo cart (use Save As)` }
  if (!fs.existsSync(pngPath)) return { ok: false, error: `no gallery ${slug}.cart.png to rebake` }
  try {
    // 1. write the Code-tab buffer back to the .c source
    fs.writeFileSync(cPath, source)
    // 2. re-embed into the gallery .cart.png, KEEPING its baked thumbnail. Mirror
    //    cart:save in-place — do NOT shell out to `make-cart.js <src> <png>`, which
    //    rebuilds the sprite sheet from the .cart.js and BLANKS a hand-drawn cart's
    //    sheet when there's no generator (make-cart.js ~line 378).
    const chunkData = { source, sprites: spritesDataUrl, map: mapBase64 }
    if (settings) chunkData.settings = JSON.stringify(settings)

    // 2b. GENERATOR carts: diff the editor sheet against the (re-run) generator and
    //     store only the changed slots as a patch beside the .c + mirrored into the
    //     .cart.png (de:spritepatch). Stateless — the diff base is the generator's
    //     current output, so there's no snapshot to keep. de:sprites stays the
    //     editor canvas (= the composited view the user drew).
    const cartJsPath   = cPath.replace(/\.c$/, '.cart.js')
    const hasGenerator = fs.existsSync(cartJsPath)
    let patchedSlots = 0, patchSlots = [], spriteError = null
    if (hasGenerator && Array.isArray(spriteSlots)) {
      try {
        const mc = require('../../tools/make-cart.js')
        const sp = require('../../tools/lib/sprite-patch.js')
        try { delete require.cache[require.resolve(cartJsPath)] } catch {}   // pick up a live .cart.js edit
        const cfg = mc.loadConfig(cPath)
        if (cfg.sprites) {
          const gen    = mc.genSlots(cfg.sprites, cfg.charMap)
          const edited = {}
          for (let s = 0; s < 64; s++) if (spriteSlots[s]) edited[s] = spriteSlots[s]
          const patch  = sp.buildPatch(slug, gen, edited)
          sp.writePatch(cPath, patch)   // writes the sibling file, or DELETES it if no slot differs
          patchSlots   = Object.keys(patch.slots).map(Number)
          patchedSlots = patchSlots.length
          if (patchedSlots) chunkData.spritepatch = sp.serializePatch(patch)
        }
      } catch (e) { spriteError = e.message }
    }

    fs.writeFileSync(pngPath, embedCartChunks(fs.readFileSync(pngPath), chunkData))
    // 3. regenerate index.json so a de:meta edit (title/tags/…) registers
    let indexError = null
    try { require('../../tools/build-cart-index.js').writeIndex() }
    catch (e) { indexError = e.message }
    return { ok: true, cRel: `tools/carts/${slug}.c`, hasGenerator, patchedSlots, patchSlots, spriteError, indexError }
  } catch (e) {
    return { ok: false, error: e.message }
  }
})

// ── discard a generator cart's sprite patch (Gap 2 / Option D) ──────
// The editor "discard hand-edits" button: delete the sibling
// tools/carts/<slug>.sprites.patch.json (the "kill" op), re-run the generator to
// rebuild the pristine sheet, and re-embed it into the gallery .cart.png WITHOUT
// the de:spritepatch chunk. Returns the fresh sprites data URL so the editor
// reloads the canvas. (Deliberately NOT a git commit.)
ipcMain.handle('cart:discard-sprite-patch', async (_event, { slug }) => {
  if (!slug || !/^[\w-]+$/.test(slug)) return { ok: false, error: 'no cart slug' }
  const ROOT      = path.join(__dirname, '../..')
  const cPath     = path.join(ROOT, 'tools', 'carts', `${slug}.c`)
  const cartJs    = cPath.replace(/\.c$/, '.cart.js')
  const patchPath = cPath.replace(/\.c$/, '.sprites.patch.json')
  const pngPath   = path.join(GALLERY_DIR, `${slug}.cart.png`)
  if (!fs.existsSync(pngPath)) return { ok: false, error: `no gallery ${slug}.cart.png` }
  try {
    const mc = require('../../tools/make-cart.js')
    if (fs.existsSync(patchPath)) fs.unlinkSync(patchPath)   // kill the patch (idempotent)
    try { delete require.cache[require.resolve(cartJs)] } catch {}
    const cfg    = mc.loadConfig(cPath)
    const pngBuf = cfg.sprites ? mc.buildSpriteSheet(cfg.sprites, cfg.charMap) : mc.makeBlankSpritePng()
    const spritesDataUrl = 'data:image/png;base64,' + pngBuf.toString('base64')
    // re-embed: keep the existing source/map/settings, rebuild sprites, DROP the
    // de:spritepatch chunk (embedCartChunks strips all de:* then writes only what we pass).
    const existing  = extractCartChunks(fs.readFileSync(pngPath))
    const chunkData = { source: existing.source, sprites: spritesDataUrl }
    if (existing.map)      chunkData.map      = existing.map
    if (existing.settings) chunkData.settings = existing.settings
    fs.writeFileSync(pngPath, embedCartChunks(fs.readFileSync(pngPath), chunkData))
    return { ok: true, spritesDataUrl }
  } catch (e) {
    return { ok: false, error: e.message }
  }
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
  return { ok: true, name, origin: inGallery(filePaths[0]) ? null : filePaths[0], source: chunks.source, spritesDataUrl: chunks.sprites || null, mapBase64: chunks.map || null, settings: parseCartSettings(chunks.settings), spritepatch: chunks.spritepatch || null }
})

ipcMain.handle('cart:load-buffer', async (_event, bytes) => {
  const chunks = extractCartChunks(Buffer.from(bytes))
  if (!chunks.source) return { ok: false, error: 'Not a dreamengine cart' }
  return { ok: true, source: chunks.source, spritesDataUrl: chunks.sprites || null, mapBase64: chunks.map || null, settings: parseCartSettings(chunks.settings), spritepatch: chunks.spritepatch || null }
})

ipcMain.handle('cart:load-file', async (_event, filePath) => {
  const chunks = extractCartChunks(fs.readFileSync(filePath))
  if (!chunks.source) return { ok: false, error: 'Not a dreamengine cart' }
  const name = path.basename(filePath).replace(/\.cart\.png$/i, '')
  return { ok: true, name, origin: inGallery(filePath) ? null : filePath, source: chunks.source, spritesDataUrl: chunks.sprites || null, mapBase64: chunks.map || null, settings: parseCartSettings(chunks.settings), spritepatch: chunks.spritepatch || null }
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
      // Flight recorder: record every native Run deterministically to the ring so any moment is
      // replayable (bug repro / clip). Skipped for net games (net owns its own --det seed via the
      // handshake) and when disabled in settings. Random per-session seed, logged into the .rec
      // header by the engine so a replay self-seeds. Design: docs/design/flight-recorder.md
      const slug = cartSlug(cfg)
      const isNet = !!(cfg?.net && cfg.net.mode)
      let recPath = null, recFlags = []
      if (cfg?.recordPlays !== false && !isNet) {
        try {
          fs.mkdirSync(ringDir(slug), { recursive: true })
          recPath = path.join(ringDir(slug), `${sessionStamp()}.rec`)
          const seed = (Math.floor(Math.random() * 0x7fffffff)) >>> 0
          recFlags = ['--det', '--seed', String(seed), '--record', recPath]
        } catch { recPath = null; recFlags = [] }
      }
      const proc = trackNative(spawn(CART_BIN, ['--title', cartTitle, ...saveDirArgs(cfg), ...netFlags, ...recFlags], { detached: false, stdio: ['ignore', 'pipe', 'pipe'], cwd: BUILD_DIR, env: cartEnv(cfg) }), cfg?.cartName, 'run', { recPath, slug })
      proc.stdout.on('data', chunk => send('cart:log', chunk.toString()))   // raylib trace (warnings only) + stray printf + net: HOSTING line
      proc.stderr.on('data', chunk => send('cart:log', chunk.toString()))   // printh() output
      proc.on('exit', (code, signal) => {
        if (recPath) {
          pruneRing(slug)
          // always tell the log where this session's recording landed — so any close leaves a
          // findable, replayable repro even if you don't keep it. The line gets a ✂ keep-take
          // button + a 📂 reveal (rlogAddLine, keyed off the "session recorded → …" text).
          try {
            if (fs.existsSync(recPath) && fs.statSync(recPath).size > 0) {
              const rel = path.relative(path.join(__dirname, '../..'), recPath)
              send('cart:log', `\n↩ session recorded → ${rel}\n  replay: node tools/play.js ${slug} replay ${rel}\n`)
            }
          } catch {}
        }
        send('cart:exit', { code, signal })
      })
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

// RECORD your own play as an input track. Same compile+spawn as studio:run, but the cart runs
// with --record <tmp>; studio.c logs input changes per frame (harness_input, native build). On
// window close we park the take at tools/clips/<slug>/NN-take.rec — the SAME home authored tracks
// use, so it feeds the whole fan-out (replay · bake a clip · reel · attract).
// Design: docs/design/input-recording-looper.md ("one track, many surfaces").
ipcMain.handle('studio:record', async (_event, code, cfg) => {
  const ROOT = path.join(__dirname, '../..')
  const dims = prepareCart(code, cfg)
  const args = macCompileArgs(dims, cfg?.buildMode === 'release' ? ['-O2', '-DDE_RELEASE'] : ['-Os'])
  const cmd  = `clang ${args.join(' ')}`
  // key the take on the CANONICAL cart id (the .cart.png basename = tools/carts/<slug>.c = tools/clips/<slug>/),
  // NOT the display title — "Squishy Lines" must land in tools/clips/squishy/, not squishy-lines/.
  const slug = (cfg?.cartFile || cfg?.cartName || 'scratch').toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-+|-+$/g, '') || 'scratch'
  const recTmp = path.join(BUILD_DIR, '.rec', 'take.rec')
  fs.mkdirSync(path.dirname(recTmp), { recursive: true })
  try { if (fs.existsSync(recTmp)) fs.unlinkSync(recTmp) } catch {}   // start clean so an empty take is detectable
  return new Promise(resolve => {
    exec(cmd, (err, _stdout, stderr) => {
      const warnings = stderr.split('\n').filter(l => !l.includes('was built for newer macOS version')).join('\n').trim()
      if (err) return resolve({ ok: false, cmd, output: warnings })
      const wc = _event.sender
      const send = (ch, payload) => { if (!wc.isDestroyed()) wc.send(ch, payload) }
      // Record deterministically (--det) so a later --replay reconstructs the SAME world:
      // a .rec stores only inputs, not the RNG stream. --replay implies --det + the default
      // seed 1, so an unseeded/wall-clock recording replays into a DIFFERENT world and diverges
      // (flank: won live, died on replay — bullets hit enemies that weren't there). Seed omitted
      // so both sides use the runtime default (1); keep them matched.
      const proc = trackNative(spawn(CART_BIN, ['--title', `${cfg?.cartName || 'dreamengine'} (● recording)`, '--det', '--record', recTmp, ...saveDirArgs(cfg)],
        { detached: false, stdio: ['ignore', 'pipe', 'pipe'], cwd: BUILD_DIR, env: cartEnv(cfg) }), cfg?.cartName, 'record')
      proc.stdout.on('data', c => send('cart:log', c.toString()))
      proc.stderr.on('data', c => send('cart:log', c.toString()))
      proc.on('error', () => {})
      proc.on('exit', () => {                                          // window closed → save the take (if any input landed)
        try {
          if (!fs.existsSync(recTmp) || fs.statSync(recTmp).size === 0) return send('cart:recorded', { ok: false, empty: true })
          const dir = path.join(ROOT, 'tools/clips', slug)
          fs.mkdirSync(dir, { recursive: true })
          const nums = fs.readdirSync(dir).map(f => { const m = f.match(/^(\d+)-/); return m ? +m[1] : 0 })
          const nn = String((nums.length ? Math.max(...nums) : 0) + 1).padStart(2, '0')
          const dest = path.join(dir, `${nn}-take.rec`)
          fs.copyFileSync(recTmp, dest)
          const rel = path.relative(ROOT, dest)
          send('cart:log', `\n✓ recorded take → ${rel}\n  replay:  node tools/play.js ${slug} replay ${rel}\n  clip:    node tools/make-gif.js ${slug} --recipe ${nn}-take\n`)   // persistent in the log panel
          send('cart:recorded', { ok: true, rel, abs: dest, slug, label: `${nn}-take` })
        } catch (e) { send('cart:recorded', { ok: false, error: String(e.message || e) }) }
      })
      resolve({ ok: true, cmd, output: warnings || null })
    })
  })
})

// REPLAY a take against the CURRENT (live-edited) cart — the mirror of studio:record.
// Same compile+spawn as studio:run; the take drives the cart deterministically. Two on-disk
// take formats play natively here (studio.c owns both flags): a .rec = a raw per-frame input
// TAPE (--replay); a .script = a hand-editable FRAME SCRIPT (--script). Both map to the cart
// they were authored on, so a foreign take just looks like noise — harmless. (A .beats is
// musical + needs a compile step → studio:play-beats, which routes through play.js instead.)
// Design: docs/design/input-recording-looper.md, docs/design/promote-tab.md.
ipcMain.handle('studio:replay', async (_event, code, cfg, takePath) => {
  const abs = String(takePath || '')
  const flag = abs.endsWith('.rec') ? '--replay' : abs.endsWith('.script') ? '--script' : null
  if (!flag || !fs.existsSync(abs)) return { ok: false, cmd: null, output: `not a readable .rec/.script take: ${abs || '(none)'}` }
  const dims = prepareCart(code, cfg)
  const args = macCompileArgs(dims, cfg?.buildMode === 'release' ? ['-O2', '-DDE_RELEASE'] : ['-Os'])
  const cmd  = `clang ${args.join(' ')}`
  return new Promise(resolve => {
    exec(cmd, (err, _stdout, stderr) => {
      const warnings = stderr.split('\n').filter(l => !l.includes('was built for newer macOS version')).join('\n').trim()
      if (err) return resolve({ ok: false, cmd, output: warnings })
      const wc = _event.sender
      const send = (ch, payload) => { if (!wc.isDestroyed()) wc.send(ch, payload) }
      const proc = trackNative(spawn(CART_BIN, ['--title', `${cfg?.cartName || 'dreamengine'} (▶ replaying)`, flag, abs, ...saveDirArgs(cfg)],
        { detached: false, stdio: ['ignore', 'pipe', 'pipe'], cwd: BUILD_DIR, env: cartEnv(cfg) }), cfg?.cartName, 'replay')
      proc.stdout.on('data', c => send('cart:log', c.toString()))
      proc.stderr.on('data', c => send('cart:log', c.toString()))
      proc.on('error', () => {})
      resolve({ ok: true, cmd, output: warnings || null })
    })
  })
})

// PLAY a .beats take — the musical take format. Unlike .rec/.script it has no native flag: play.js
// compiles the beat-script into frame events, so this shells out to it. Consequence worth knowing:
// it runs the ON-DISK cart (tools/carts/<cart>.c), NOT the live editor buffer. Design: promote-tab.md.
ipcMain.handle('studio:play-beats', async (_event, cart, beatsPath) => {
  const ROOT = path.join(__dirname, '../..')
  if (!/^[a-z0-9_-]+$/i.test(cart || '')) return { ok: false, output: 'bad cart name' }
  const abs = String(beatsPath || '')
  if (!abs.endsWith('.beats') || !fs.existsSync(abs)) return { ok: false, output: `not a readable .beats take: ${abs || '(none)'}` }
  const wc = _event.sender
  const send = (ch, payload) => { if (!wc.isDestroyed()) wc.send(ch, payload) }
  const proc = spawn('node', [path.join(ROOT, 'tools/play.js'), String(cart), 'beats', abs], { cwd: ROOT })
  proc.stdout.on('data', c => send('cart:log', c.toString()))
  proc.stderr.on('data', c => send('cart:log', c.toString()))
  proc.on('error', e => send('cart:log', String(e.message) + '\n'))
  return { ok: true }
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

// ATTACH-profile a cart you're ALREADY playing (no respawn, no kill) — the answer to
// "profile the state I played into, not a cold startup" (STATUS #45). Two lenses on the
// LIVE pid: (a) the live-inspection mailbox dumps a fresh perf JSON (the self-instrumented
// per-primitive work view, compiled into EVERY native build — no -DDE_PROFILE needed),
// (b) /usr/bin/sample walks the call-tree without stopping the cart. Returns the same
// { hotspots, perf } shape the cold profiler does, so renderProfile() renders it unchanged.
async function runProfileAttach(pid, cfg) {
  const target = pid ? nativeCarts.get(pid) : runningCarts()[0]
  if (!target || !target.proc || target.proc.killed || !target.proc.pid)
    return { ok: false, profile: true, output: 'no running cart to attach to — press ▶ Run first' }
  const tpid = target.proc.pid
  const seconds = cfg?.profileSeconds || 4

  // (a) fresh on-demand perf dump FROM THIS pid via the .bake mailbox (pid-targeted so a
  // sibling cart polling the same file can't serve the wrong frame — the CLAUDE.md hazard)
  const bake = path.join(BUILD_DIR, '.bake')
  try { fs.mkdirSync(bake, { recursive: true }) } catch {}
  const perfOut = path.join(bake, 'perf-attach.json')
  try { fs.unlinkSync(perfOut) } catch {}
  try { fs.writeFileSync(path.join(bake, 'profiler_request'), `${perfOut}\npid:${tpid}\n`) } catch {}
  // Generous window: a backgrounded cart (App Nap when the editor is focused) polls
  // slowly, so give it room before falling back.
  const served = await waitForConsumed(path.join(bake, 'profiler_request'), 5000)

  // (b) sample the SAME live pid for the call-tree — do NOT kill it
  const sampleFile = path.join(BUILD_DIR, 'profile-attach.txt')
  let report = ''
  try {
    await execP(`/usr/bin/sample ${tpid} ${seconds} -file "${sampleFile}"`)
    report = fs.readFileSync(sampleFile, 'utf8')
  } catch (e) {
    return { ok: false, profile: true, output: `sample failed (needs permission to attach?): ${e.message}` }
  }
  // Frame-timing: the on-demand mailbox dump (fresh snapshot) is best; if the cart was
  // throttled and didn't serve it in time, fall back to its own perf.json (auto-flushed
  // every 30 frames — same running cart, slightly less fresh). Null only if neither exists.
  let perf = served ? readPerfFrom(perfOut) : null
  if (!perf) perf = readPerf()
  return {
    ok: true, profile: true, attached: true, seconds, cartName: target.name,
    hotspots: parseSample(report),
    perf,
    perfNote: perf ? undefined
      : 'frame-timing unavailable — the cart didn’t serve the profiler mailbox (window backgrounded/minimised, or a release build). Tip: focus the cart window and press ⌘⇧P.',
  }
}
ipcMain.handle('studio:profile-attach', (_event, pid, cfg) => runProfileAttach(pid, cfg))

// Triggered from the Debug menu / global shortcut (main-side, no renderer round-trip in) —
// run the attach and push the result to the renderer's build log.
async function profileAttachFromMenu(pid) {
  const win = BrowserWindow.getAllWindows()[0]
  if (!win || win.isDestroyed()) return
  const result = await runProfileAttach(pid, {})
  if (!win.isDestroyed()) win.webContents.send('profile:attached', result)
}

// Keep (promote) the flight-recorder session of a running cart into tools/clips. pid null → the
// most-recently-started cart. Driven by the Debug menu / global ⌘⇧K; logs the replay + clip commands.
function keepTakeFromMenu(pid) {
  const target = pid ? nativeCarts.get(pid) : runningCarts()[0]
  const win = BrowserWindow.getAllWindows()[0]
  const send = (ch, p) => { if (win && !win.isDestroyed()) win.webContents.send(ch, p) }
  if (!target || !target.recPath) return send('cart:log', '\n(no running cart with a recording to keep — is recording enabled?)\n')
  keepTake(target.recPath, target.slug || cartSlug({ cartName: target.name }), send)
}
ipcMain.handle('studio:record-keep', (_e, pid) => { keepTakeFromMenu(pid); return { ok: true } })

// Keep a take straight from its ring FILE — the ✂ keep-take button on the console's
// "session recorded" line (works AFTER the cart has closed, when there's no live pid). Path must
// live under build/.rec; slug is its directory. Logs the replay + clip commands (→ 🎬 bake button).
ipcMain.handle('studio:keep-take-file', (_e, recRel) => {
  const ROOT = path.join(__dirname, '../..')
  const abs = path.isAbsolute(String(recRel || '')) ? String(recRel) : path.join(ROOT, String(recRel || ''))
  const ringRoot = path.join(BUILD_DIR, '.rec')
  if (!abs.startsWith(ringRoot) || !fs.existsSync(abs)) return { ok: false, error: 'not a ring take' }
  const win = BrowserWindow.getAllWindows()[0]
  const send = (ch, p) => { if (win && !win.isDestroyed()) win.webContents.send(ch, p) }
  return keepTake(abs, path.basename(path.dirname(abs)), send)
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
// reveal a repo file in Finder (accepts an absolute path or a repo-relative one) — used by the record toast
ipcMain.handle('studio:reveal-path', (_e, p) => {
  const ROOT = path.join(__dirname, '../..')
  const abs = path.isAbsolute(String(p || '')) ? String(p) : path.join(ROOT, String(p || ''))
  if (abs.startsWith(ROOT) && fs.existsSync(abs)) shell.showItemInFolder(abs)
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
        const products = (m.iap && Array.isArray(m.iap.products)) ? m.iap.products : []
        // IAP display name + description are their OWN searchable ASO surface (Apple indexes them),
        // so hand back the copy, not just a count. Limits: name 30, desc 45.
        const iapProducts = products.map(p => ({ id: p.id || '', name: p.name || '', desc: p.desc || '', price: p.price || '' }))
        apps.push({ dir, name: m.name || dir, carts: m.carts || [], launcher: m.launcher || '',
          iap: products.length, iapProducts, listing: m.listing || null })
      } catch {}
    }
    return { ok: true, apps }
  } catch (e) { return { ok: false, apps: [], output: String(e.message || e) } }
})

// Derive research SEED TERMS for an app. Two sources:
//   'meta' (default) — the carts' de:meta via the generated index.json: teaches[] tags are the
//     honest CATEGORY terms (hyphens → spaces: "drum-synthesis" → "drum synthesis"), titles the
//     brand-y fallback. This is "explore the landscape my app lives in."
//   'listing' — the terms you ACTUALLY CHOSE: the manifest's keyword-field entries + notable
//     title/subtitle words. This is "grade my listing against the competition" (the 🔬 analyze
//     button) — research runs on your own bets, so you see which are crowded and who ranks.
// Returns a small, editable list the Apps view drops into the research box (stays maker-editable).
ipcMain.handle('studio:app-seeds', async (_e, name, source = 'meta') => {
  const ROOT = path.join(__dirname, '../..')
  const dedupCap = (arr, n) => {
    const seen = new Set(), out = []
    for (const t of arr) { const k = t.toLowerCase(); if (t && !seen.has(k)) { seen.add(k); out.push(t) } }
    return out.slice(0, n)
  }
  try {
    const m = JSON.parse(fs.readFileSync(path.join(ROOT, 'apps', name, 'app.json'), 'utf8'))
    const appInfo = { name: m.name || name, title: (m.listing || {}).title || m.name || name }
    if (source === 'listing') {
      const L = m.listing || {}
      const words = s => String(s || '').toLowerCase().split(/[^a-z0-9]+/).filter(w => w.length >= 3)
      // keyword-field entries first (your deliberate bets), then subtitle words, then title words
      const kw = String(L.keywords || '').split(',').map(s => s.trim()).filter(Boolean)
      const terms = dedupCap([...kw, ...words(L.subtitle), ...words(L.title)], 12)
      if (!terms.length) return { ok: false, error: 'no listing yet — add a "listing" block to app.json', terms: [] }
      return { ok: true, terms, app: appInfo }
    }
    const idx = JSON.parse(fs.readFileSync(path.join(ROOT, 'editor/public/carts/index.json'), 'utf8'))
    const byFile = new Map(idx.map(c => [c.file, c]))
    const norm = s => String(s || '').toLowerCase().replace(/[-_]+/g, ' ').replace(/\s+/g, ' ').trim()
    const teaches = [], titles = []
    for (const cart of (m.carts || [])) {
      const e = byFile.get(`${cart}.cart.png`)
      if (!e) continue
      for (const t of (e.teaches || [])) { const n = norm(t); if (n) teaches.push(n) }
      const ti = norm(e.title); if (ti) titles.push(ti)
    }
    return { ok: true, terms: dedupCap([...teaches, ...titles], 8), app: appInfo }
  } catch (e) { return { ok: false, error: String(e.message || e), terms: [] } }
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
ipcMain.handle('studio:aso-suggest', async (_e, terms, country) => {
  const list = String(terms || '').split(',').map(s => s.trim()).filter(Boolean)
  if (!list.length) return { ok: false, error: 'type at least one seed term' }
  const cc = /^[a-z]{2}$/i.test(country || '') ? String(country).toLowerCase() : 'us'
  const ROOT = path.join(__dirname, '../..')
  return new Promise(resolve => {
    let out = '', err = ''
    const proc = spawn('node', [path.join(ROOT, 'tools/aso-suggest.js'), '--json', '--country', cc, ...list], { cwd: ROOT })
    proc.stdout.on('data', c => out += c.toString())
    proc.stderr.on('data', c => err += c.toString())
    proc.on('exit', code => {
      if (code !== 0) return resolve({ ok: false, error: err.trim() || 'suggest failed' })
      try { resolve({ ok: true, data: JSON.parse(out) }) }
      catch { resolve({ ok: false, error: 'could not parse suggestions' }) }
    })
    proc.on('error', e => resolve({ ok: false, error: String(e.message) }))
  })
})
// app-scoped: build the committed SEO worksheet (writes apps/<name>/seo-brief.md), and the
// mirror that checks the finished copy against it. Both stream to the aso log panel.
ipcMain.handle('studio:aso-brief', async (_e, name) => {
  const res = await runAsoTool(_e, 'aso-brief.js', [String(name)])
  // hand back the absolute path so the editor can render a clickable "open" link (open-path
  // validates it's inside ROOT). rel is shown to the user; abs is what shell.openPath needs.
  return { ...res, path: path.join(__dirname, '../..', 'apps', String(name), 'seo-brief.md'), rel: `apps/${name}/seo-brief.md` }
})
ipcMain.handle('studio:aso-coverage', (_e, name) => runAsoTool(_e, 'aso-coverage.js', [String(name)]))
// score the committed listing (--deep = winnability, hits the network). Returns parsed JSON so the
// editor can render the scorecard WITH its gotchas. The iterative A/B tweak loop stays in the CLI.
ipcMain.handle('studio:aso-score', async (_e, name) => {
  const ROOT = path.join(__dirname, '../..')
  return new Promise(resolve => {
    let out = '', err = ''
    const proc = spawn('node', [path.join(ROOT, 'tools/aso-score.js'), String(name), '--deep', '--json'], { cwd: ROOT })
    proc.stdout.on('data', c => out += c.toString())
    proc.stderr.on('data', c => err += c.toString())
    proc.on('exit', code => {
      try { resolve({ ok: true, data: JSON.parse(out) }) }
      catch { resolve({ ok: false, error: err.trim() || `score failed (exit ${code})` }) }
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

// ── leads (demand GENERATION): where do I post about this app's carts? ─────────
// The generation twin of the ASO capture tools. leads is per-CART, an app is many carts, so this
// runs `leads.js match <cart> --json` for each cart of the app and aggregates. Returns tribes +
// venues + a ready-to-copy post scaffold per cart; the maker does the actual posting (we prep, they paste).
function runLeadsJson(cart) {
  const ROOT = path.join(__dirname, '../..')
  return new Promise(resolve => {
    let out = '', err = ''
    const proc = spawn('node', [path.join(ROOT, 'tools/leads.js'), 'match', String(cart), '--json'], { cwd: ROOT })
    proc.stdout.on('data', c => out += c.toString())
    proc.stderr.on('data', c => err += c.toString())
    proc.on('exit', () => { try { resolve(JSON.parse(out)) } catch { resolve({ ok: false, cart, error: (err.trim() || 'no de:meta / not a cart') }) } })
    proc.on('error', e => resolve({ ok: false, cart, error: String(e.message) }))
  })
}
ipcMain.handle('studio:leads', async (_e, name) => {
  const ROOT = path.join(__dirname, '../..')
  let carts = []
  try { carts = JSON.parse(fs.readFileSync(path.join(ROOT, 'apps', String(name), 'app.json'), 'utf8')).carts || [] }
  catch (e) { return { ok: false, error: `no apps/${name}/app.json (${e.message})` } }
  if (!carts.length) return { ok: false, error: 'app.json has no carts' }
  const per = await Promise.all(carts.map(runLeadsJson))
  // cross-cutting is domain-wide (same for every music cart) — surface it once, from the first hit
  const crosscutting = per.find(p => p.ok && p.crosscutting?.length)?.crosscutting || []
  return { ok: true, app: String(name), carts: per, crosscutting }
})

// ── Promote tab (per-cart) ─────────────────────────────────────
// Cart-scoped twins of studio:leads / studio:app-clips, for the open cart's Promote tab.
// Design: docs/design/promote-tab.md. Both reuse the app-scoped logic for ONE cart.
// cart-leads: wrap runLeadsJson(cart) in the {carts:[…], crosscutting} shape renderLeads expects.
ipcMain.handle('studio:cart-leads', async (_e, cart) => {
  if (!/^[a-z0-9_-]+$/i.test(cart || '')) return { ok: false, error: 'bad cart name' }
  const one = await runLeadsJson(cart)
  return { ok: true, carts: [one], crosscutting: (one && one.ok && one.crosscutting) || [] }
})
// cart-clips: this cart's takes (tools/clips/<cart>/*.{rec,script,beats}) + baked webm
// (editor/public/clips/<cart>/*.webm), plus the deterministic gallery URL for section E.
// Every take format is playable, so each take exposes a playKind + playPath: .rec/.script play
// natively via studio:replay, .beats via studio:play-beats. Preference rec > script > beats when
// a label has more than one (a recorded tape is the most faithful; beats needs a compile).
ipcMain.handle('studio:cart-clips', async (_e, cart) => {
  const ROOT = path.join(__dirname, '../..')
  if (!/^[a-z0-9_-]+$/i.test(cart || '')) return { ok: false, error: 'bad cart name' }
  const pub = path.join(ROOT, 'editor/public/clips', cart)
  // split native clips (<label>.webm) from per-ratio VARIANTS (<label>--<W>x<H>.webm, Stage 2)
  const baked = new Set(), variants = {}
  for (const f of (fs.existsSync(pub) ? fs.readdirSync(pub).filter(f => f.endsWith('.webm')) : [])) {
    const vm = f.match(/^(.*)--(\d+x\d+)\.webm$/)
    if (vm) (variants[vm[1]] || (variants[vm[1]] = [])).push(vm[2])
    else baked.add(f.replace(/\.webm$/, ''))
  }
  const recDir = path.join(ROOT, 'tools/clips', cart)
  const takes = {}
  const takeFiles = fs.existsSync(recDir) ? fs.readdirSync(recDir).filter(f => /\.(script|beats|rec)$/.test(f)) : []
  for (const f of takeFiles) {
    const label = f.replace(/\.(script|beats|rec)$/, ''); const ext = f.split('.').pop()
    const t = takes[label] || (takes[label] = { kinds: [], paths: {} })
    t.kinds.push(ext); t.paths[ext] = path.join(recDir, f)
  }
  const PREF = ['rec', 'script', 'beats']
  const labels = [...new Set([...baked, ...Object.keys(takes)])].sort()
  const clips = labels.map(l => {
    const t = takes[l]
    const playKind = t ? (PREF.find(k => t.paths[k]) || null) : null
    return {
      label: l, baked: baked.has(l),
      clipUrl: baked.has(l) ? `/clips/${cart}/${l}.webm` : null,
      kinds: t ? t.kinds : [],
      variants: (variants[l] || []).sort(),   // per-ratio baked sizes, e.g. ["886x1920","180x320"]
      playKind, playPath: playKind ? t.paths[playKind] : null,
    }
  })
  // per-cart stills (§B): plain PNGs a stranger would post, editor/public/shots/<cart>/*.png
  const shotsDir = path.join(ROOT, 'editor/public/shots', cart)
  const shots = fs.existsSync(shotsDir)
    ? fs.readdirSync(shotsDir).filter(f => /\.png$/.test(f)).sort().map(f => `/shots/${cart}/${f}`)
    : []
  return { ok: true, cart, clips, shots, url: `https://nikkikoole.github.io/dreamengine/${cart}/` }
})
// bake a take → a shareable clip. Runs make-gif.js --recipe <label>, which reads the take
// tools/clips/<cart>/<label>.{rec,script,beats} and writes editor/public/clips/<cart>/<label>.webm
// (deterministic: same recipe → same clip, with audio). Streams progress to the runtime log.
// The "produce" verb of the Promote tab made concrete. Design: docs/design/promote-tab.md §A.
// A `size` (WxH) bakes a per-ratio VARIANT → editor/public/clips/<cart>/<label>--<WxH>.webm, which a
// same-ratio reel FILLS with (export-ratios.md), AND is a standalone clip you can post (single-clip
// export). No size = the native <label>.webm. The variant is CART-TYPE AWARE (the export-ratios "fix"):
//  · RESIZABLE cart → make-gif --screen WxH: the cart reflows/lays out for the target canvas → FILLS.
//  · FIXED-layout cart → render native, then ffmpeg LETTERBOX into WxH (black bars, crisp nearest) —
//    because --screen on a fixed cart just crops the world + strands the HUD (a broken frame, not 9:16).
ipcMain.handle('studio:bake-clip', async (_e, cart, label, size) => {
  const ROOT = path.join(__dirname, '../..')
  if (!/^[a-z0-9_-]+$/i.test(cart || '')) return { ok: false, output: 'bad cart name' }
  if (!/^[a-z0-9][a-z0-9_.-]*$/i.test(label || '')) return { ok: false, output: 'bad take label' }
  const wc = _e.sender
  const send = (ch, payload) => { if (!wc.isDestroyed()) wc.send(ch, payload) }
  const variant = typeof size === 'string' && /^\d+x\d+$/.test(size)
  const nativeRel = `editor/public/clips/${cart}/${label}.webm`
  const runMakeGif = (extra, outRel) => new Promise(resolve => {
    const proc = spawn('node', [path.join(ROOT, 'tools/make-gif.js'), String(cart), '--recipe', String(label), ...extra, ...(outRel ? ['--out', outRel] : [])], { cwd: ROOT })
    proc.stdout.on('data', c => send('cart:log', c.toString()))
    proc.stderr.on('data', c => send('cart:log', c.toString()))
    proc.on('exit', code => resolve(code === 0))
    proc.on('error', e => { send('cart:log', String(e.message) + '\n'); resolve(false) })
  })
  if (!variant) {
    send('cart:log', `\n── bake ${cart}/${label} → clip ──\n`)
    const ok = await runMakeGif([], nativeRel)
    return { ok, out: ok ? nativeRel : null }
  }
  const variantRel = `editor/public/clips/${cart}/${label}--${size}.webm`
  const srcPath = path.join(ROOT, 'tools/carts', `${cart}.c`)
  const resizable = fs.existsSync(srcPath) && /"resizable"\s*:\s*true/.test(fs.readFileSync(srcPath, 'utf8'))
  if (resizable) {   // reflow-fill: the cart lays out for the target canvas
    send('cart:log', `\n── bake ${cart}/${label} @ ${size} (reflow-fill) → clip ──\n`)
    const ok = await runMakeGif(['--screen', size], variantRel)
    return { ok, out: ok ? variantRel : null, mode: 'fill' }
  }
  // fixed-layout: render native once, then letterbox it into WxH (black bars, crisp)
  send('cart:log', `\n── bake ${cart}/${label} @ ${size} (letterbox — fixed layout) → clip ──\n`)
  if (!fs.existsSync(path.join(ROOT, nativeRel))) {
    send('cart:log', `  (baking native ${label} first)\n`)
    if (!await runMakeGif([], nativeRel)) return { ok: false, output: 'native bake failed' }
  }
  const [W, H] = size.split('x').map(Number)
  // integer nearest-upscale that FITS the native into the frame (never downscale — that's the blur),
  // then pad. n≥1; a 320×200 game into 720×1280 → ×2 = 640×400 centred with bars, crisp.
  let n = 3
  try {
    const pr = spawnSync('ffprobe', ['-v', 'error', '-select_streams', 'v:0', '-show_entries', 'stream=width,height', '-of', 'csv=s=x:p=0', path.join(ROOT, nativeRel)], { encoding: 'utf8' })
    const [nw, nh] = String(pr.stdout || '').trim().split('x').map(Number)
    if (nw && nh) n = Math.max(1, Math.floor(Math.min(W / nw, H / nh)))
  } catch {}
  const vf = `scale=iw*${n}:ih*${n}:flags=neighbor,pad=${W}:${H}:(ow-iw)/2:(oh-ih)/2:color=black,setsar=1`
  return new Promise(resolve => {
    const ff = spawn('ffmpeg', ['-y', '-i', path.join(ROOT, nativeRel), '-vf', vf, '-c:v', 'libvpx-vp9', '-crf', '30', '-b:v', '0', '-pix_fmt', 'yuv420p', '-c:a', 'copy', path.join(ROOT, variantRel)], { cwd: ROOT })
    let err = ''
    ff.stderr.on('data', c => { err += c.toString() })
    ff.on('exit', code => { if (code === 0) { send('cart:log', `✓ letterboxed → ${variantRel}\n`); resolve({ ok: true, out: variantRel, mode: 'letterbox' }) } else { send('cart:log', err.split('\n').slice(-4).join('\n') + '\n'); resolve({ ok: false, output: 'ffmpeg letterbox failed — see log' }) } })
    ff.on('error', e => resolve({ ok: false, output: String(e.message) }))
  })
})
// snapshot the current cart → a plain PNG still (editor/public/shots/<cart>/NN-snap.png), a frame
// worth posting. Runs play.js headless, dumps ~40 frames, keeps the middle one (past boot). These
// are per-cart stills — NOT the device-sized App Store shots (store-shots.js, app-scope). §B.
ipcMain.handle('studio:cart-shot', async (_e, cart) => {
  const ROOT = path.join(__dirname, '../..')
  if (!/^[a-z0-9_-]+$/i.test(cart || '')) return { ok: false, output: 'bad cart name' }
  const wc = _e.sender
  const send = (ch, payload) => { if (!wc.isDestroyed()) wc.send(ch, payload) }
  const scratch = path.join(ROOT, 'build', '.shot', cart)
  fs.rmSync(scratch, { recursive: true, force: true }); fs.mkdirSync(scratch, { recursive: true })
  send('cart:log', `\n── snapshot ${cart} ──\n`)
  const ok = await new Promise(resolve => {
    const proc = spawn('node', [path.join(ROOT, 'tools/play.js'), String(cart), 'run', '--headless', '--frames', '40', '--dump', scratch], { cwd: ROOT })
    proc.stdout.on('data', c => send('cart:log', c.toString()))
    proc.stderr.on('data', c => send('cart:log', c.toString()))
    proc.on('exit', code => resolve(code === 0))
    proc.on('error', e => { send('cart:log', String(e.message) + '\n'); resolve(false) })
  })
  if (!ok) return { ok: false, output: 'capture failed — see the log' }
  let frames = []
  try { frames = fs.readdirSync(scratch).filter(f => /\.png$/.test(f)).sort() } catch {}
  if (!frames.length) return { ok: false, output: 'no frames captured' }
  const pick = frames[Math.floor(frames.length / 2)]
  const shotsDir = path.join(ROOT, 'editor/public/shots', cart)
  fs.mkdirSync(shotsDir, { recursive: true })
  const nums = fs.readdirSync(shotsDir).map(f => parseInt(f, 10)).filter(n => !isNaN(n))
  const nn = String((nums.length ? Math.max(...nums) : 0) + 1).padStart(2, '0')
  const rel = `shots/${cart}/${nn}-snap.png`
  fs.copyFileSync(path.join(scratch, pick), path.join(ROOT, 'editor/public', rel))
  send('cart:log', `✓ still → editor/public/${rel}\n`)
  return { ok: true, url: `/${rel}` }
})

// ── App Store metadata push (the ☁︎ App Store panel) ─────────────
// Two modes, both via tools/asc-push.js --json so the panel can render a structured checklist:
//   default (no opts.push)   → --dry-run: GET live + diff → a PLAN the panel shows (read-only, safe)
//   opts.push = [fields]      → --only <fields>: PATCH ONLY those fields live (the outward write)
// The two-click ceremony lives in the UI: click 1 opens the panel (dry-run), click 2 pushes the
// ticked fields. Metadata-only for now (screenshots / IAP are separate channels, added later).
ipcMain.handle('studio:asc-metadata', async (_e, name, opts = {}) => {
  if (!/^[a-z0-9_-]+$/i.test(name || '')) return { ok: false, error: 'bad app name' }
  const ROOT = path.join(__dirname, '../..')
  const push = Array.isArray(opts?.push) ? opts.push.filter(f => /^[A-Za-z]+$/.test(f)) : null
  const args = [path.join(ROOT, 'tools/asc-push.js'), String(name), '--metadata', '--json']
  if (push && push.length) args.push('--only', push.join(','))     // real push of the ticked fields
  else args.push('--dry-run')                                       // read-only plan
  return new Promise(resolve => {
    let out = '', err = ''
    const proc = spawn('node', args, { cwd: ROOT })
    proc.stdout.on('data', c => out += c.toString())
    proc.stderr.on('data', c => err += c.toString())
    proc.on('exit', code => {
      try { resolve({ ok: true, data: JSON.parse(out), pushed: !!(push && push.length) }) }
      catch { resolve({ ok: false, error: (err.trim() || `asc-push failed (exit ${code})`).replace(/^✗\s*/, '') }) }
    })
    proc.on('error', e => resolve({ ok: false, error: String(e.message) }))
  })
})

// Promoted-purchases channel of the same ☁︎ panel — twin of asc-metadata but for `--promote`.
// Separate handler because `--only` here takes PRODUCT IDS (dotted, e.g. com.x.acidrack), not field names.
ipcMain.handle('studio:asc-promote', async (_e, name, opts = {}) => {
  if (!/^[a-z0-9_-]+$/i.test(name || '')) return { ok: false, error: 'bad app name' }
  const ROOT = path.join(__dirname, '../..')
  const push = Array.isArray(opts?.push) ? opts.push.filter(id => /^[A-Za-z0-9._-]+$/.test(id)) : null
  const args = [path.join(ROOT, 'tools/asc-push.js'), String(name), '--promote', '--json']
  if (push && push.length) args.push('--only', push.join(','))     // promote only the ticked IAPs
  else args.push('--dry-run')                                       // read-only plan
  return new Promise(resolve => {
    let out = '', err = ''
    const proc = spawn('node', args, { cwd: ROOT })
    proc.stdout.on('data', c => out += c.toString())
    proc.stderr.on('data', c => err += c.toString())
    proc.on('exit', code => {
      try { resolve({ ok: true, data: JSON.parse(out), pushed: !!(push && push.length) }) }
      catch { resolve({ ok: false, error: (err.trim() || `asc-push failed (exit ${code})`).replace(/^✗\s*/, '') }) }
    })
    proc.on('error', e => resolve({ ok: false, error: String(e.message) }))
  })
})

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
    proc.on('exit', code => {
      // reveal the built Mac .app in Finder + drop a clickable link (build/<manifest name>.app)
      if (code === 0 && target === 'mac') {
        try {
          const m = JSON.parse(fs.readFileSync(path.join(ROOT, 'apps', name, 'app.json'), 'utf8'))
          const appPath = path.join(ROOT, 'build', (m.name || name) + '.app')
          if (fs.existsSync(appPath)) { try { shell.showItemInFolder(appPath) } catch {} ; log(`\n✓ open: file://${encodeURI(appPath)}\n`) }
        } catch {}
      }
      resolve({ ok: code === 0 })
    })
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
      if (code === 0 && fs.existsSync(out)) log(`\npreview: file://${encodeURI(out)}\n`)   // clickable in the log
      resolve({ ok: code === 0 })
    })
    proc.on('error', e => { log(String(e.message) + '\n'); resolve({ ok: false }) })
  })
})

// ── trailer builder (docs/design/trailer-builder.md) ──────────
// app-clips: the LIBRARY — each rack's committed clips (baked webm and/or a recipe) + the current
// tools/reels/<name>.reel parsed into rows, so the builder opens pre-populated (never blank).
// ── shared reel/clip helpers (app-clips · reel-load · list-reels) ────────────────
// clips available for a cart: baked webms (editor/public/clips/<cart>/) ∪ recipes (tools/clips/<cart>/).
function reelClipsFor(ROOT, cart) {
  const pub = path.join(ROOT, 'editor/public/clips', cart)
  // base clips only — per-ratio variants (<label>--<W>x<H>.webm) aren't draggable clips; compose
  // picks them by the reel's output size (export-ratios.md Stage 2).
  const baked = new Set(fs.existsSync(pub) ? fs.readdirSync(pub).filter(f => f.endsWith('.webm') && !/--\d+x\d+\.webm$/.test(f)).map(f => f.replace(/\.webm$/, '')) : [])
  const recDir = path.join(ROOT, 'tools/clips', cart)
  const recs = fs.existsSync(recDir) ? fs.readdirSync(recDir).filter(f => /\.(script|beats|rec)$/.test(f)).map(f => f.replace(/\.(script|beats|rec)$/, '')) : []
  return [...new Set([...baked, ...recs])].sort().map(l => ({ label: l, clip: `${cart}/${l}`, baked: baked.has(l) }))
}
// parse a .reel manifest → { rows, loop } in the trailer builder's timeline shape (rows=null if no file).
function parseReelFile(reelPath) {
  if (!fs.existsSync(reelPath)) return { rows: null, loop: null }
  const text = fs.readFileSync(reelPath, 'utf8')
  const lm = text.match(/^#\s*loop\s+(\w+)\s+([\d.]+)/m)
  const loop = lm ? { type: lm[1], dur: +lm[2] } : null
  const sm = text.match(/^#\s*size\s+(\d+x\d+)/m)   // output aspect/size the reel was built at
  const size = sm ? sm[1] : null
  const rows = []
  const parseLines = (parts) => parts.reduce((acc, seg) => {   // pull title/sub/body role-lines out of a segment list
    const cm = seg.match(/^(title|sub|body)\s+(.*)$/); if (cm) acc.push({ role: cm[1], text: cm[2].replace(/^"(.*)"$/, '$1') }); return acc
  }, [])
  for (const line of text.split('\n')) {
    const t = line.trim(); if (!t || t.startsWith('#')) continue
    if (t.startsWith('@card')) {   // a generated text-card part
      const parts = t.split('|').map(s => s.trim())
      const row = { card: true, dur: parseFloat(parts[0].split(/\s+/)[1]) || 2.0, lines: parseLines(parts.slice(1)), anim: 'fade', bg: null, xtype: 'fade', xdur: 0.5 }
      for (const seg of parts.slice(1)) {
        let cm
        if      ((cm = seg.match(/^anim\s+(.+)$/)))       row.anim = cm[1]
        else if ((cm = seg.match(/^in\s+([\d.]+)\s+(.+)$/)))  { row.inDur = +cm[1]; row.inEffect = cm[2] }
        else if ((cm = seg.match(/^out\s+([\d.]+)\s+(.+)$/))) { row.outDur = +cm[1]; row.outEffect = cm[2] }
        else if ((cm = seg.match(/^wait\s+([\d.]+)\s+([\d.]+)/))) { row.waitBefore = +cm[1]; row.waitAfter = +cm[2] }
        else if ((cm = seg.match(/^bg\s+(\d+)/)))         row.bg = +cm[1]
        else if ((cm = seg.match(/^boil\s+([\d.]+)/)))    row.boil = +cm[1]
        else if ((cm = seg.match(/^breathe\s+([\d.]+)/))) row.breathe = +cm[1]
        else if ((cm = seg.match(/^bpm\s+([\d.]+)/)))     row.bpm = +cm[1]
        else if ((cm = seg.match(/^(\w+)\s+([\d.]+)/)) && !/^(title|sub|body)$/.test(cm[1])) { row.xtype = cm[1]; row.xdur = +cm[2] }
      }
      row.inEffect = row.inEffect || row.anim || 'fade'   // wait/in/hold/out/wait for the editor (hold derived)
      if (row.inDur == null) row.inDur = 0.5
      if (row.outDur == null) row.outDur = 0
      row.outEffect = row.outEffect || 'slide top'
      row.waitBefore = row.waitBefore || 0
      row.waitAfter = row.waitAfter || 0
      row.holdDur = Math.max(0, (row.dur || 2) - row.waitBefore - row.inDur - row.outDur - row.waitAfter)
      rows.push(row); continue
    }
    if (t.startsWith('over')) {   // a text overlay riding the preceding row
      const prev = rows[rows.length - 1]; if (!prev) continue
      const parts = t.split('|').map(s => s.trim())
      const win = parts[0].match(/@\s*([\d.]+)\s*-\s*([\d.]+)/)
      const ov = { a: win ? +win[1] : 0, b: win ? +win[2] : 0, pos: 'bottom', anim: 'fade', lines: parseLines(parts.slice(1)) }
      for (const seg of parts.slice(1)) {
        let cm
        if      ((cm = seg.match(/^anim\s+(.+)$/)))       ov.anim = cm[1]
        else if ((cm = seg.match(/^in\s+([\d.]+)\s+(.+)$/)))  { ov.inDur = +cm[1]; ov.inEffect = cm[2] }
        else if ((cm = seg.match(/^out\s+([\d.]+)\s+(.+)$/))) { ov.outDur = +cm[1]; ov.outEffect = cm[2] }
        else if ((cm = seg.match(/^wait\s+([\d.]+)\s+([\d.]+)/))) { ov.waitBefore = +cm[1]; ov.waitAfter = +cm[2] }
        else if ((cm = seg.match(/^pos\s+(\w+)/)))        ov.pos = cm[1]
        else if ((cm = seg.match(/^boil\s+([\d.]+)/)))    ov.boil = +cm[1]
        else if ((cm = seg.match(/^breathe\s+([\d.]+)/))) ov.breathe = +cm[1]
        else if ((cm = seg.match(/^bpm\s+([\d.]+)/)))     ov.bpm = +cm[1]
      }
      ;(prev.overlays || (prev.overlays = [])).push(ov); continue
    }
    const [ref, ...segs] = t.split('|').map(s => s.trim())     // ref + any `| verb …` segments (order-independent)
    const row = { clip: ref, xtype: 'fade', xdur: 0.5, trim: null, speed: 1 }
    for (const seg of segs) {
      let sm
      if ((sm = seg.match(/^trim\s+([\d.]+)\s+([\d.]+)/)))  row.trim = [+sm[1], +sm[2]]
      else if ((sm = seg.match(/^speed\s+([\d.]+)/)))       row.speed = +sm[1]
      else if ((sm = seg.match(/^(\w+)\s+([\d.]+)/)))     { row.xtype = sm[1]; row.xdur = +sm[2] }
    }
    rows.push(row)
  }
  return { rows, loop, size }
}
ipcMain.handle('studio:app-clips', async (_e, name) => {
  const ROOT = path.join(__dirname, '../..')
  if (!/^[a-z0-9_-]+$/i.test(name || '')) return { ok: false, error: 'bad app name' }
  try {
    const m = JSON.parse(fs.readFileSync(path.join(ROOT, 'apps', name, 'app.json'), 'utf8'))
    const carts = (m.carts || []).map(c => ({ cart: c, clips: reelClipsFor(ROOT, c) }))
    const { rows, loop, size } = parseReelFile(path.join(ROOT, 'tools/reels', `${name}.reel`))
    return { ok: true, name: m.name || name, carts, rows, loop, size }
  } catch (e) { return { ok: false, error: String(e.message || e) } }
})
// list-reels: every saved scenario (tools/reels/*.reel) + whether it has a built webm. Drives the
// "saved reels" list in the trailer builder — click one to reel-load it. docs/design/trailer-builder.md.
ipcMain.handle('studio:list-reels', async () => {
  const ROOT = path.join(__dirname, '../..')
  const dir = path.join(ROOT, 'tools/reels')
  if (!fs.existsSync(dir)) return { ok: true, reels: [] }
  const reels = fs.readdirSync(dir).filter(f => f.endsWith('.reel')).map(f => f.replace(/\.reel$/, '')).sort()
    .map(nm => ({ name: nm, hasWebm: fs.existsSync(path.join(ROOT, 'editor/public/reels', `${nm}.webm`)) }))
  return { ok: true, reels }
})
// reel-load: parse a saved scenario → rows/loop + a library of every cart its clips reference, so
// the trailer builder can open ANY saved reel (not just the one named after the current subject).
ipcMain.handle('studio:reel-load', async (_e, name) => {
  const ROOT = path.join(__dirname, '../..')
  if (!/^[a-z0-9_-]+$/i.test(name || '')) return { ok: false, error: 'bad reel name' }
  const { rows, loop, size } = parseReelFile(path.join(ROOT, 'tools/reels', `${name}.reel`))
  if (!rows) return { ok: false, error: `no tools/reels/${name}.reel` }
  const carts = [...new Set(rows.filter(r => !r.card && r.clip).map(r => String(r.clip).split('/')[0]))]
    .map(c => ({ cart: c, clips: reelClipsFor(ROOT, c) }))
  return { ok: true, name, carts, rows, loop, size }
})
// build-reel: NON-DESTRUCTIVE — write the ordered rows to tools/reels/<name>.reel (a parameter
// list, sources untouched), bake any referenced clip that isn't baked yet, then compose → the
// baked reel the editor previews. rows: [{clip:"cart/label", xtype, xdur}] in order.
ipcMain.handle('studio:build-reel', async (_e, name, rows, loop, size) => {
  const wc = _e.sender
  const log = (m) => { if (!wc.isDestroyed()) wc.send('aso:log', m) }
  const ROOT = path.join(__dirname, '../..')
  if (!/^[a-z0-9_-]+$/i.test(name || '')) return { ok: false, error: 'bad app name' }
  rows = (Array.isArray(rows) ? rows : []).filter(r => r && (r.card || (typeof r.clip === 'string' && /^[a-z0-9_-]+\/[a-z0-9_.-]+$/i.test(r.clip))))
  if (!rows.length) return { ok: false, error: 'add at least one clip' }
  log(`\n── build trailer: ${name} (${rows.length} part${rows.length > 1 ? 's' : ''}) ──\n`)
  // which clips are missing a baked .webm? bake only those, with a [k/N] counter (the slow phase).
  // (cards aren't clips — compose-clips bakes them from the titlecard cart.)
  const need = [...new Set(rows.filter(r => !r.card).map(r => r.clip))]
    .filter(clip => { const [cart, label] = clip.split('/'); return !fs.existsSync(path.join(ROOT, 'editor/public/clips', cart, `${label}.webm`)) })
  if (need.length) log(`baking ${need.length} missing clip${need.length > 1 ? 's' : ''}…\n`)
  else log(`all clips already baked — composing\n`)
  let k = 0
  for (const clip of need) {                                         // bake any missing clip (never mutates sources)
    const [cart, label] = clip.split('/')
    log(`  [${++k}/${need.length}] baking ${clip}…\n`)
    if (!await spawnP('node', [path.join(ROOT, 'tools/make-gif.js'), cart, '--recipe', label], ROOT, log))
      return { ok: false, error: `bake failed: ${clip}` }
  }
  const reelPath = path.join(ROOT, 'tools/reels', `${name}.reel`)
  let reel = `# ${name} — built by the trailer builder (docs/design/trailer-builder.md)\n# fps 30\n# xfade fade 0.5\n`
  if (typeof size === 'string' && /^\d+x\d+$/.test(size)) {   // output aspect/size (compose-clips letterboxes mismatched clips onto it)
    // output EXACTLY the picked (crisp, full-size) canvas — scale 1, no hidden ×3 upscale. Clips
    // baked at this ratio fill it edge-to-edge; mismatched clips letterbox (nearest, still crisp).
    reel += `# size ${size}\n# scale 1\n`
  }
  if (loop && loop.type && loop.dur > 0) reel += `# loop ${loop.type} ${loop.dur}\n`   // seamless loop-close
  const lineFor = (r, i) => {   // one row → one or more .reel lines (a clip/card + its overlay continuation lines)
    const cut = i > 0 ? [`${r.xtype || 'fade'} ${r.xdur || 0.5}`] : []
    if (r.card) {               // @card <dur> | <cut> | title/sub/body | anim | bg
      const inDur = r.inDur != null ? r.inDur : 0.5
      const outDur = r.outDur || 0
      const wb = r.waitBefore || 0, wa = r.waitAfter || 0
      const holdDur = r.holdDur != null ? r.holdDur : Math.max(0, (r.dur || 2) - inDur - outDur)
      const total = Math.round((wb + inDur + holdDur + outDur + wa) * 100) / 100   // @card carries the total
      const segs = [...cut]
      for (const l of (r.lines || [])) segs.push(`${l.role} "${l.text}"`)
      segs.push(`in ${inDur} ${r.inEffect || r.anim || 'fade'}`)
      if (outDur > 0) segs.push(`out ${outDur} ${r.outEffect || 'slide top'}`)
      if (wb > 0 || wa > 0) segs.push(`wait ${wb} ${wa}`)
      if (r.bg != null) segs.push(`bg ${r.bg}`)
      if (r.boil != null) segs.push(`boil ${r.boil}`)
      if (r.breathe != null) segs.push(`breathe ${r.breathe}`)
      if (r.bpm > 0) segs.push(`bpm ${r.bpm}`)
      return [`@card ${total}${segs.length ? ' | ' + segs.join(' | ') : ''}`]
    }
    const segs = [...cut]       // a clip: cut | trim | speed
    if (Array.isArray(r.trim) && r.trim.length === 2) segs.push(`trim ${r.trim[0]} ${r.trim[1]}`)
    if (r.speed && +r.speed !== 1) segs.push(`speed ${+r.speed}`)
    const out = [segs.length ? `${r.clip} | ${segs.join(' | ')}` : r.clip]
    for (const ov of (r.overlays || [])) {   // over @a-b | pos | anim | title/sub/body
      const os = []
      if (ov.pos) os.push(`pos ${ov.pos}`)
      if (ov.inDur != null) os.push(`in ${ov.inDur} ${ov.inEffect || 'fade'}`)
      if (ov.outDur) os.push(`out ${ov.outDur} ${ov.outEffect || 'slide top'}`)
      if (ov.waitBefore > 0 || ov.waitAfter > 0) os.push(`wait ${ov.waitBefore || 0} ${ov.waitAfter || 0}`)
      if (ov.anim && ov.inDur == null) os.push(`anim ${ov.anim}`)
      if (ov.boil != null) os.push(`boil ${ov.boil}`)
      if (ov.breathe != null) os.push(`breathe ${ov.breathe}`)
      if (ov.bpm > 0) os.push(`bpm ${ov.bpm}`)
      for (const l of (ov.lines || [])) os.push(`${l.role} "${l.text}"`)
      out.push([`over @${ov.a}-${ov.b}`, ...os].join(' | '))
    }
    return out
  }
  reel += rows.flatMap((r, i) => lineFor(r, i)).join('\n') + '\n'
  fs.mkdirSync(path.dirname(reelPath), { recursive: true })
  fs.writeFileSync(reelPath, reel)
  log(`wrote tools/reels/${name}.reel\n`)
  if (rows.length < 2) return { ok: true, reel: `tools/reels/${name}.reel`, out: null, note: 'add a 2nd clip to compose a reel' }
  if (!await spawnP('node', [path.join(ROOT, 'tools/compose-clips.js'), name], ROOT, log)) return { ok: false, error: 'compose failed' }
  log(`✓ editor/public/reels/${name}.webm\n`)
  return { ok: true, reel: `tools/reels/${name}.reel`, out: `reels/${name}.webm` }   // web path (Vite serves public/)
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
