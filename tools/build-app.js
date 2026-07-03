// build-app.js — build a MULTI-CART APP binary from an app manifest (apps/<name>/app.json).
// The umbrella-app build-ladder rung 2 (docs/design/share-panel.md §ladder): generates
// everything tools/bundle-spike/build.sh hardcoded, so ADDING A RACK = ONE MANIFEST LINE.
//
//   node tools/build-app.js tinyjam            # apps/tinyjam/app.json → build/tinyjam
//   node tools/build-app.js apps/tinyjam       # same
//   node tools/build-app.js tinyjam --run      # build then launch it
//
// Manifest (apps/<name>/app.json — the committed home per share-panel.md open-q #1 +
// ADR-0026's metadata-next-to-manifest layout):
//   { "name": "Tinyjam", "bundleId": "com.dreamengine.tinyjam", "version": "0.1",
//     "carts": ["acidrack", "yachtrack"],          // tools/carts/<c>.c, in context order
//     "launcher": "...", "icon": "...", "iap": {}  // parked: rung 3 / mac-app / iOS rungs
//   }
//
// What it does:
//   1. Validates: carts exist, count ≤ SOUND_CART_CTX (parsed live from runtime/sound.h),
//      every cart's screen/cell/map dims MATCH (share-panel.md next-spike #3: the manifest
//      picks ONE size — no letterboxing yet).
//   2. Stages sprites/map from the FIRST cart that has any (per-cart sheets are a later
//      rung — share-panel.md next-spike #1; fine for code-drawn racks).
//   3. Compiles each cart TU with -Ddraw=<slug>_draw -Dupdate=<slug>_update
//      -Dinit=<slug>_init -Dspec=<slug>_spec (everything else in a cart is `static`,
//      so the TUs link clean side by side), then asks `nm` which optional entry points
//      (init/update/spec) the cart actually defines.
//   4. Generates the dispatcher shim: TAB cycles carts, each switch = de_switch_cart(ctx)
//      (the per-cart sound context, ADR-0027 / rung 1) + a one-time init.
//      DE_BUNDLE_AUTOSWITCH=<N> auto-cycles every N frames — the headless proof hook.
//   5. Links with studio.c + raylib → build/<app name in the manifest, lowercased>.
//
// Verify a build: DE_BUNDLE_AUTOSWITCH=400 build/<name> --headless + live screenshots,
// and tools/bundle-spike/proof-sound.sh covers the underlying context mechanism.
// The old hand-written spike (bundle-spike/build.sh) stays as the minimal reference.
'use strict'
const fs = require('fs')
const path = require('path')
const { execFileSync } = require('child_process')
const mk = require(path.join(__dirname, 'make-cart.js'))

const ROOT = path.join(__dirname, '..')

// ── args ─────────────────────────────────────────────────────────────────────
const args = process.argv.slice(2)
const run = args.includes('--run')
const target = args.find(a => !a.startsWith('--'))
if (!target) {
  console.error('usage: node tools/build-app.js <app-name | apps/<name>[/app.json]> [--run]')
  process.exit(1)
}

// ── load manifest ────────────────────────────────────────────────────────────
let manifestPath = target
if (!manifestPath.endsWith('app.json')) manifestPath = path.join(manifestPath, 'app.json')
if (!fs.existsSync(path.join(ROOT, manifestPath)) && !path.isAbsolute(manifestPath))
  manifestPath = path.join('apps', target, 'app.json')
const manifestFile = path.isAbsolute(manifestPath) ? manifestPath : path.join(ROOT, manifestPath)
if (!fs.existsSync(manifestFile)) {
  console.error(`no manifest at ${manifestPath} — expected apps/<name>/app.json (try: ls apps/)`)
  process.exit(1)
}
const app = JSON.parse(fs.readFileSync(manifestFile, 'utf8'))
if (!app.name || !Array.isArray(app.carts) || app.carts.length === 0) {
  console.error('manifest needs at least: { "name": "...", "carts": ["cart", ...] }')
  process.exit(1)
}
for (const k of ['launcher', 'iap', 'icon'])
  if (app[k]) console.log(`note: manifest "${k}" is parked for a later rung — accepted, unused today`)

// ── cap: contexts the engine actually has (never drifts from sound.h) ─────────
const soundH = fs.readFileSync(path.join(ROOT, 'runtime/sound.h'), 'utf8')
const ctxCap = parseInt((soundH.match(/#define\s+SOUND_CART_CTX\s+(\d+)/) || [])[1], 10)
if (!ctxCap) { console.error('could not parse SOUND_CART_CTX from runtime/sound.h'); process.exit(1) }
if (app.carts.length > ctxCap) {
  console.error(`${app.carts.length} carts > SOUND_CART_CTX (${ctxCap}) — raise it in runtime/sound.h first`)
  process.exit(1)
}

// ── per-cart config + the one-screen-size rule ───────────────────────────────
const slugOf = c => (/^\d/.test(c) ? 'c_' + c : c).replace(/[^A-Za-z0-9]/g, '_')
const carts = app.carts.map(c => {
  const src = path.join(ROOT, 'tools/carts', c + '.c')
  if (!fs.existsSync(src)) { console.error(`cart not found: tools/carts/${c}.c`); process.exit(1) }
  const cfg = mk.loadConfig(src)
  return { name: c, slug: slugOf(c), src, cfg }
})
const dims = c => ({
  screenW: c.cfg.screenW ?? 320, screenH: c.cfg.screenH ?? 200, scale: c.cfg.scale ?? 4,
  cellW: c.cfg.cellW ?? 16, cellH: c.cfg.cellH ?? 16, mapW: c.cfg.mapW ?? 128, mapH: c.cfg.mapH ?? 64,
})
const d0 = dims(carts[0])
for (const c of carts.slice(1)) {
  const d = dims(c)
  for (const k of ['screenW', 'screenH', 'cellW', 'cellH', 'mapW', 'mapH'])
    if (d[k] !== d0[k]) {
      console.error(`screen/grid mismatch: ${carts[0].name} ${k}=${d0[k]} but ${c.name} ${k}=${d[k]}`)
      console.error('a manifest picks ONE size and its carts must match (share-panel.md next-spike #3)')
      process.exit(1)
    }
}

// ── staging ──────────────────────────────────────────────────────────────────
const appId = app.name.toLowerCase().replace(/[^a-z0-9]/g, '')
const stage = path.join(ROOT, 'build', '.app-' + appId)
fs.mkdirSync(stage, { recursive: true })

// sheet/map: first cart that brings any (per-cart sheets are a later rung)
const withSprites = carts.find(c => c.cfg.sprites)
if (carts.filter(c => c.cfg.sprites).length > 1)
  console.log(`note: ${carts.filter(c => c.cfg.sprites).length} carts have sprite configs — staging ${withSprites.name}'s sheet for ALL (per-cart sheets = a later rung)`)
const sheetBuf = withSprites ? mk.buildSpriteSheet(withSprites.cfg.sprites, withSprites.cfg.charMap) : mk.makeBlankSpritePng()
const withMap = carts.find(c => c.cfg.map)
const mapBytes = withMap ? mk.buildMap(withMap.cfg.map.layout || withMap.cfg.map, withMap.cfg.map.tiles, withMap.cfg.mapW, withMap.cfg.mapH) : new Uint8Array(8192)
const cHeader = (sym, buf) => {
  let s = `static const unsigned char ${sym}[] = {\n`
  for (let i = 0; i < buf.length; i += 16)
    s += '  ' + Array.from(buf.slice(i, i + 16)).join(', ') + ',\n'
  return s + `};\nstatic const unsigned int ${sym}_LEN = ${buf.length};\n`
}
fs.writeFileSync(path.join(stage, 'sprites_data.h'),
  cHeader('SPRITES_DATA', sheetBuf).replace('SPRITES_DATA_LEN', 'SPRITES_DATA_LEN'))
fs.writeFileSync(path.join(stage, 'map_data.h'),
  cHeader('MAP_DATA', Buffer.from(mapBytes)))

// studio.c includes them with these exact names — keep symbol spellings identical to make-cart:
// SPRITES_DATA/SPRITES_DATA_LEN and MAP_DATA/MAP_DATA_LEN (the cHeader emits <sym>_LEN).

// ── compile each cart TU with renamed entry points ───────────────────────────
const COMMON = [
  '-I' + path.join(ROOT, 'runtime'), '-I' + stage, '-I' + path.join(mk.RAYLIB, 'include'),
  `-DSCREEN_W=${d0.screenW}`, `-DSCREEN_H=${d0.screenH}`, `-DSCALE=${d0.scale}`,
  `-DMAP_W=${d0.mapW}`, `-DMAP_H=${d0.mapH}`, `-DCELL_W=${d0.cellW}`, `-DCELL_H=${d0.cellH}`,
  '-DTOUCH_CONTROLS_DEFAULT=0', '-DSCALE_FILTER=0', '-O2', '-fno-delete-null-pointer-checks',
]
const clang = (a) => execFileSync('clang', a, { stdio: ['ignore', 'pipe', 'pipe'] })
const objs = []
for (const c of carts) {
  const staged = path.join(stage, c.slug + '.c')
  fs.copyFileSync(c.src, staged)                     // provenance: the staged copy is what compiled
  const obj = path.join(stage, c.slug + '.o')
  clang(['-c', staged, '-o', obj, ...COMMON,
    `-Ddraw=${c.slug}_draw`, `-Dupdate=${c.slug}_update`,
    `-Dinit=${c.slug}_init`, `-Dspec=${c.slug}_spec`])
  const nm = execFileSync('nm', ['-gU', obj], { encoding: 'utf8' })
  c.has = {
    draw:   nm.includes(` _${c.slug}_draw`),
    update: nm.includes(` _${c.slug}_update`),
    init:   nm.includes(` _${c.slug}_init`),
  }
  if (!c.has.draw) { console.error(`${c.name} defines no draw() — every cart needs one`); process.exit(1) }
  objs.push(obj)
}

// ── generate the dispatcher shim ─────────────────────────────────────────────
const N = carts.length
const decls = carts.map(c => {
  let s = `void ${c.slug}_draw(void);`
  s += c.has.update ? ` void ${c.slug}_update(void);` : ` static void ${c.slug}_update(void) {}`
  s += c.has.init   ? ` void ${c.slug}_init(void);`   : ` static void ${c.slug}_init(void) {}`
  return s
}).join('\n')
const shim = `// GENERATED by tools/build-app.js from ${path.relative(ROOT, manifestFile)} — do not edit.
// Dispatcher shim for the "${app.name}" app: owns the real init/update/draw and forwards
// to the active cart. Each switch = de_switch_cart(ctx) — the per-cart sound context
// (ADR-0027): instruments, bus FX, wave tables, bpm all restore per cart.
// TAB cycles carts (rung 3 replaces this with a launcher cart fed by de:meta).
// DE_BUNDLE_AUTOSWITCH=<N> cycles every N frames — the deterministic headless proof hook.
#include "studio.h"
#include <stdlib.h>

${decls}

typedef struct { void (*init)(void); void (*update)(void); void (*draw)(void); } AppCart;
static const AppCart carts[${N}] = {
${carts.map(c => `    { ${c.slug}_init, ${c.slug}_update, ${c.slug}_draw },   // ctx ${carts.indexOf(c)}: ${c.name}`).join('\n')}
};
static int active = 0;
static int booted[${N}] = { 0 };
static int autoswitch = 0;

static void activate(int i) {
    if (i == active) return;
    de_switch_cart(i);            // engine swaps the whole sound world (panic + restore included)
    active = i;
    if (!booted[i]) { booted[i] = 1; carts[i].init(); }
}

void init(void) {
    const char *a = getenv("DE_BUNDLE_AUTOSWITCH");
    if (a) autoswitch = atoi(a);
    booted[0] = 1;
    carts[0].init();
}

void update(void) {
    if (keyp(KEY_TAB)) activate((active + 1) % ${N});
    if (autoswitch && frame() > 0 && frame() % autoswitch == 0)
        activate((active + 1) % ${N});
    carts[active].update();
}

void draw(void) {
    carts[active].draw();
}
`
const shimFile = path.join(stage, 'app_main.c')
fs.writeFileSync(shimFile, shim)
const shimObj = path.join(stage, 'app_main.o')
clang(['-c', shimFile, '-o', shimObj, ...COMMON])

// ── link ─────────────────────────────────────────────────────────────────────
const out = path.join(ROOT, 'build', appId)
try {
  clang([shimObj, ...objs, path.join(ROOT, 'runtime/studio.c'), ...COMMON,
    path.join(mk.RAYLIB, 'lib/libraylib.a'),
    '-framework', 'OpenGL', '-framework', 'Cocoa', '-framework', 'IOKit',
    '-framework', 'CoreVideo', '-framework', 'CoreFoundation', '-framework', 'CoreMIDI',
    '-Wl,-dead_strip', '-o', out])
} catch (e) {
  const err = (e.stderr || '').toString().split('\n')
    .filter(l => !l.includes('was built for newer macOS version')).join('\n')
  console.error(err || e.message)
  process.exit(1)
}

console.log(`✓ built build/${appId}  (${d0.screenW}x${d0.screenH} @${d0.scale}x)`)
carts.forEach((c, i) => console.log(`    ctx ${i}: ${c.name}${c.has.init ? '' : '  (no init)'}${c.has.update ? '' : '  (no update)'}`))
console.log('  TAB cycles carts · DE_BUNDLE_AUTOSWITCH=<frames> auto-cycles (headless proof)')

if (run) {
  const { spawn } = require('child_process')
  spawn(out, [], { cwd: path.join(ROOT, 'build'), stdio: 'inherit', detached: true }).unref()
  console.log(`▶ launched build/${appId}`)
}
