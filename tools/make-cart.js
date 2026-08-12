#!/usr/bin/env node
// Usage: node tools/make-cart.js <source.c> <output.cart.png>
//
// Env: DE_RUNTIME_DIR=<dir>   compile against a DIFFERENT engine tree instead of runtime/. Also honoured
//      by play.js (which uses this file as a lib), which makes it the "render against the PRE-change
//      engine" gate: copy runtime/, restore the file you touched from git into the copy, render the same
//      cart with the same harness args against both, compare shas. Turns "byte-identical" into a
//      measurement without a destructive `git checkout` on a hot shared header. See
//      docs/guides/debug-harness.md → "A/B against the pre-change engine".

const fs   = require('fs')
const path = require('path')
const zlib = require('zlib')
const spritePatch = require('./lib/sprite-patch.js')

const ROOT_DIR    = path.join(__dirname, '..')
const BUILD_DIR   = path.join(ROOT_DIR, 'build')
// DE_RUNTIME_DIR points the compile at a DIFFERENT engine tree — the "render against the PRE-change
// engine" gate. Copy runtime/, restore the file you touched from git, and the same cart + same harness
// args build against both, so "byte-identical" is a measurement instead of an argument. Defaults to ours.
const RUNTIME_DIR = process.env.DE_RUNTIME_DIR
  ? path.resolve(process.env.DE_RUNTIME_DIR)
  : path.join(ROOT_DIR, 'runtime')
const RAYLIB      = fs.existsSync('/opt/homebrew/opt/raylib') ? '/opt/homebrew/opt/raylib' : '/usr/local/opt/raylib'
const CART_BIN    = path.join(BUILD_DIR, 'cart')

// ── PNG chunk helpers ─────────────────────────────────────────
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
  const lenBuf  = Buffer.allocUnsafe(4); lenBuf.writeUInt32BE(data.length)
  const crcBuf  = Buffer.allocUnsafe(4); crcBuf.writeUInt32BE(crc32(Buffer.concat([typeBuf, data])))
  return Buffer.concat([lenBuf, typeBuf, data, crcBuf])
}

function makeZtxtChunk(keyword, text) {
  const compressed = zlib.deflateSync(Buffer.from(text, 'utf8'))
  return makeChunk('zTXt', Buffer.concat([
    Buffer.from(keyword, 'latin1'), Buffer.from([0, 0]), compressed,
  ]))
}

const PNG_SIG = Buffer.from([137, 80, 78, 71, 13, 10, 26, 10])

function makePng(w, h, drawPixel) {
  const scanlines = Buffer.alloc(h * (1 + w * 3))
  for (let y = 0; y < h; y++) {
    scanlines[y * (1 + w * 3)] = 0  // filter: None
    for (let x = 0; x < w; x++) {
      const [r, g, b] = drawPixel(x, y)
      const i = y * (1 + w * 3) + 1 + x * 3
      scanlines[i] = r; scanlines[i + 1] = g; scanlines[i + 2] = b
    }
  }
  const ihdr = Buffer.alloc(13)
  ihdr.writeUInt32BE(w, 0); ihdr.writeUInt32BE(h, 4)
  ihdr[8] = 8; ihdr[9] = 2  // 8-bit RGB
  return Buffer.concat([
    PNG_SIG,
    makeChunk('IHDR', ihdr),
    makeChunk('IDAT', zlib.deflateSync(scanlines)),
    makeChunk('IEND', Buffer.alloc(0)),
  ])
}

// RGBA twin of makePng, for sheets that need REAL transparency rather than a colorkey.
// drawPixel returns [r,g,b,a]. The engine treats alpha < 128 as a hole on BOTH paths (the
// software canvas tests the packed pixel's top bit; the GPU shader discards texel.a < 0.5),
// so an alpha sheet needs no reserved palette index and no colorkey() call in the cart.
// Used by the atlas path below, where cells are arbitrary rects with genuine gaps between
// them — a colorkey would have to burn a palette entry that a model might legitimately want.
function makePngRGBA(w, h, drawPixel) {
  const scanlines = Buffer.alloc(h * (1 + w * 4))
  for (let y = 0; y < h; y++) {
    scanlines[y * (1 + w * 4)] = 0  // filter: None
    for (let x = 0; x < w; x++) {
      const [r, g, b, a] = drawPixel(x, y)
      const i = y * (1 + w * 4) + 1 + x * 4
      scanlines[i] = r; scanlines[i + 1] = g; scanlines[i + 2] = b; scanlines[i + 3] = a
    }
  }
  const ihdr = Buffer.alloc(13)
  ihdr.writeUInt32BE(w, 0); ihdr.writeUInt32BE(h, 4)
  ihdr[8] = 8; ihdr[9] = 6  // 8-bit RGBA
  return Buffer.concat([
    PNG_SIG,
    makeChunk('IHDR', ihdr),
    makeChunk('IDAT', zlib.deflateSync(scanlines)),
    makeChunk('IEND', Buffer.alloc(0)),
  ])
}

// pico-8 palette (the base 16)
const P = [
  [0x00,0x00,0x00], [0x1d,0x2b,0x53], [0x7e,0x25,0x53], [0x00,0x87,0x51],
  [0xab,0x52,0x36], [0x5f,0x57,0x4f], [0xc2,0xc3,0xc7], [0xff,0xf1,0xe8],
  [0xff,0x00,0x4d], [0xff,0xa3,0x00], [0xff,0xec,0x27], [0x00,0xe4,0x36],
  [0x29,0xad,0xff], [0x83,0x76,0x9c], [0xff,0x77,0xa8], [0xff,0xcc,0xaa],
]

// the full pico32 palette (indices 0–31): the base 16 above + the 16 extended
// darks/extras. ONE source of truth — buildSpriteSheet and sprite-preview.js both
// read it, so a palette tweak never drifts between the sheet and its preview.
const PAL32 = [...P,
  [0x29,0x18,0x14],[0x11,0x1d,0x35],[0x42,0x21,0x36],[0x12,0x53,0x59],
  [0x74,0x2f,0x29],[0x49,0x33,0x3b],[0xa2,0x88,0x79],[0xf3,0xef,0x7d],
  [0xbe,0x12,0x50],[0xff,0x6c,0x24],[0xa8,0xe7,0x2e],[0x00,0xb5,0x43],
  [0x06,0x5a,0xb5],[0x75,0x46,0x65],[0xff,0x6e,0x59],[0xff,0x9d,0x81],
]

function makePlaceholderPng() {
  return makePng(320, 200, (x, y) => {
    if (y < 8) return P[12]   // blue header bar
    return P[1]               // dark blue background
  })
}

function makeBlankSpritePng() {
  return makePng(128, 128, () => P[0])  // all black
}

// ── sprite sheet builder ──────────────────────────────────────
// sprites: { slotIndex: pixelArray }
// pixelArray: flat 256-element array of palette indices (16×16, row-major)
// OR a multi-line string where each char maps via charMap to a palette index
//
// default charMap covers the most useful pico-8 colors:
const DEFAULT_CHAR_MAP = {
  '.':0, '0':0,
  '1':1,  '2':2,  '3':3,  '4':4,  '5':5,  '6':6,  '7':7,
  '8':8,  '9':9,  'a':10, 'b':11, 'c':12, 'd':13, 'e':14, 'f':15,
  // friendly aliases
  'K':0,  // blacK
  'B':1,  // dark Blue
  'P':2,  // dark Purple
  'G':3,  // dark Green
  'N':4,  // browN
  'S':5,  // dark grey (Slate)
  'L':6,  // Light grey
  'W':7,  // White
  'R':8,  // Red
  'O':9,  // Orange
  'Y':10, // Yellow
  'g':11, // bright Green
  'b':12, // bright Blue
  'I':13, // Indigo
  'k':14, // pinK
  'p':15, // Peach
}

function parseSprite(src, charMap = DEFAULT_CHAR_MAP) {
  if (Array.isArray(src)) return src.slice(0, 256)
  // string: strip leading/trailing blank lines, split into chars
  const rows = src.split('\n').map(l => l.trimEnd()).filter(l => l.length > 0)
  const pixels = []
  for (let y = 0; y < 16; y++) {
    const row = rows[y] || ''
    for (let x = 0; x < 16; x++) {
      pixels.push(charMap[row[x]] ?? 0)
    }
  }
  return pixels
}

// Parse a cart's { slot: pixelArrayOrString } sprites into per-slot palette-index
// arrays ({ slotIndex: [256 indices] }) — the generator's pristine output, which
// the sprite-patch core fingerprints + composites against (Gap 2, Option D).
// ── sheet size ────────────────────────────────────────────────
// A cart's sheet defaults to 128×128 — an 8×8 grid of 16×16 slots, 64 of them — which is
// what the in-editor sprite canvas draws and what every cart shipped before 2026-08.
//
// A `.cart.js` may declare a BIGGER one (`sheet: { w, h }`, or `sheet: 256` for square).
// The runtime never cared: `spr()` derives its column count from the loaded sheet's real
// width, and `sspr()` addresses any sub-rect. The 128 was only ever baked into this tool.
// It became a blocker for baked-rotation art, where one object costs eight cells and a
// realistic set runs past 64,000 pixels (docs/design/iso-rooms.md §7).
//
// THE COST, and it is why this is opt-in rather than the new default: the sprite editor
// assumes the 8×8/16×16 grid, so a wide-sheet cart cannot be pixel-edited in the editor and
// is generator-only. That is already the standing rule for `.cart.js` generator carts
// (CLAUDE.md, "two sources of truth that don't know about each other"), so it costs nothing
// new — but it does mean you should not declare a big sheet unless you need one.
const SLOT = 16, DEFAULT_SHEET = 128

function resolveSheet(sheet) {
  if (sheet == null) return { w: DEFAULT_SHEET, h: DEFAULT_SHEET }
  const s = typeof sheet === 'number' ? { w: sheet, h: sheet } : sheet
  const w = s.w | 0, h = s.h | 0
  if (w % SLOT || h % SLOT || w <= 0 || h <= 0) {
    throw new Error(`sheet must be positive multiples of ${SLOT}, got ${w}x${h}`)
  }
  return { w, h }
}

function genSlots(sprites, charMap, sheet) {
  // a cart's charMap EXTENDS the defaults (so it only needs to declare the extra
  // chars it uses, e.g. {M:28}); it does not replace them. Cart entries win on conflict.
  const map = charMap ? { ...DEFAULT_CHAR_MAP, ...charMap } : DEFAULT_CHAR_MAP
  const { w, h } = resolveSheet(sheet)
  const cap = (w / SLOT) * (h / SLOT)                 // 64 on the default sheet
  const slots = {}
  for (const [slot, src] of Object.entries(sprites)) {
    const idx = parseInt(slot)
    if (idx >= 0 && idx < cap) slots[idx] = parseSprite(src, map)
  }
  return slots
}

// Render per-slot palette-index arrays ({ slot: [256] }) to a sprite sheet PNG
// (16×16 slots, row-major, columns = sheet.w/16). The one place slot pixels become an
// image, so the generator output and the patched-composite output take the identical path.
function slotsToSheetPng(slots, sheet) {
  const { w: SW, h: SH } = resolveSheet(sheet)
  const COLS = SW / SLOT
  const pixels = new Array(SW * SH).fill(0)
  for (const [slot, parsed] of Object.entries(slots)) {
    const idx = parseInt(slot)
    const ox  = (idx % COLS) * SLOT
    const oy  = Math.floor(idx / COLS) * SLOT
    for (let py = 0; py < SLOT; py++) {
      for (let px = 0; px < SLOT; px++) {
        pixels[(oy + py) * SW + (ox + px)] = parsed[py * SLOT + px] || 0
      }
    }
  }
  return makePng(SW, SH, (x, y) => PAL32[pixels[y * SW + x]] || [0,0,0])
}

// Render a RAW ATLAS to the sheet PNG: { w, h, px } where px is a flat array of palette
// indices and anything < 0 (or null/undefined) is TRANSPARENT. This is the escape from the
// slot grid — cells are arbitrary rects the cart addresses with sspr(), which is what baked
// rotation sets need, since a sofa's cell is 64×32 at one angle and 48×40 at another.
// Written with alpha rather than a colorkey so no palette index has to be sacrificed.
function atlasToSheetPng(atlas) {
  const { w, h, px } = atlas
  if (!Number.isInteger(w) || !Number.isInteger(h) || w <= 0 || h <= 0) {
    throw new Error(`atlas needs positive integer w/h, got ${w}x${h}`)
  }
  if (!px || px.length < w * h) throw new Error(`atlas px must hold w*h = ${w * h} entries, got ${px ? px.length : 0}`)
  return makePngRGBA(w, h, (x, y) => {
    const v = px[y * w + x]
    if (v == null || v < 0) return [0, 0, 0, 0]
    const c = PAL32[v] || [0, 0, 0]
    return [c[0], c[1], c[2], 255]
  })
}

function buildSpriteSheet(sprites, charMap, sheet) {
  return slotsToSheetPng(genSlots(sprites, charMap, sheet), sheet)
}

// The ONE place a .cart.js config becomes a sheet PNG: raw atlas, else slot grid, else blank.
// Every consumer that stages build/sprites.png must go through this — play.js, the editor and
// make-cart each used to re-derive it, and the moment `atlas` arrived play.js silently shipped
// a BLANK (all-black) sheet, which renders as black furniture rather than as an error.
function sheetBufFor(cfg) {
  if (!cfg) return makeBlankSpritePng()
  if (cfg.atlas)   return atlasToSheetPng(cfg.atlas)
  if (cfg.sprites) return buildSpriteSheet(cfg.sprites, cfg.charMap, cfg.sheet)
  return makeBlankSpritePng()
}

// ── map builder ───────────────────────────────────────────────
// layout: array of strings (ASCII art rows)
// tiles:  { 'char': tileIndex, ... }  default: '#'→1, '.'→0, ' '→0
function buildMap(layout, tiles = {}, mapW = 128, mapH = 64) {
  const tileMap = { '.': 0, ' ': 0, '#': 1, ...tiles }
  const data    = new Uint8Array(mapW * mapH)
  for (let y = 0; y < Math.min(layout.length, mapH); y++) {
    const row = layout[y]
    for (let x = 0; x < Math.min(row.length, mapW); x++) {
      data[y * mapW + x] = tileMap[row[x]] ?? 0
    }
  }
  return data
}

function embedCartChunks(pngBuf, data) {
  const parts = [PNG_SIG]
  let offset = 8, iend = null
  while (offset + 12 <= pngBuf.length) {
    const len  = pngBuf.readUInt32BE(offset)
    const type = pngBuf.slice(offset + 4, offset + 8).toString('ascii')
    const chunk = pngBuf.slice(offset, offset + 12 + len)
    if (type === 'IEND') { iend = chunk; break }
    // Drop any de:* zTXt chunks already in the source PNG — we re-write them
    // below. This makes embedCartChunks idempotent, so it's safe to re-embed
    // onto an already-baked cart.png (e.g. preserving its thumbnail image)
    // without piling up duplicate de:source/de:sprites chunks.
    if (type === 'zTXt' && pngBuf.slice(offset + 8, offset + 11).toString('latin1') === 'de:') {
      offset += 12 + len
      continue
    }
    parts.push(chunk)
    offset += 12 + len
  }
  for (const [key, val] of Object.entries(data)) parts.push(makeZtxtChunk(`de:${key}`, val))
  if (iend) parts.push(iend)
  return Buffer.concat(parts)
}

// ── config loader ─────────────────────────────────────────────
// looks for a .cart.js file alongside the .c source
// exports: { sprites, map, charMap, mapW, mapH, sheet?, atlas? }
//   sheet: { w, h } | n   — sheet size, multiples of 16, default 128×128 (see resolveSheet)
//   atlas: { w, h, px }   — a RAW atlas of palette indices, bypassing the slot grid
function loadConfig(srcFile) {
  const cfgFile = srcFile.replace(/\.c$/, '.cart.js')
  if (!fs.existsSync(cfgFile)) return {}
  try {
    return require(path.resolve(cfgFile))
  } catch (e) {
    console.warn('warning: could not load', cfgFile, e.message)
    return {}
  }
}

// ── bake a cart's sprite sheet (generator + optional hand-patch) ─────
// Runs the .cart.js generator, then composites any sibling
// tools/carts/<name>.sprites.patch.json on top (Gap 2, Option D): hand-edited
// slots survive the bake, stale ones are dropped LOUDLY + pruned from the file,
// and the surviving patch is returned so the caller can mirror it into the
// .cart.png as a de:spritepatch chunk. Returns { pngBuf, patchJson|null }.
function bakeSprites(srcFile, cfg) {
  // A raw atlas bypasses the slot grid entirely (and therefore the hand-patch subsystem,
  // which is slot-addressed by definition — there is nothing to patch when cells are
  // arbitrary rects). Generator-only by construction.
  if (cfg.atlas) return { pngBuf: atlasToSheetPng(cfg.atlas), patchJson: null }
  if (!cfg.sprites) return { pngBuf: makeBlankSpritePng(), patchJson: null }
  const gen   = genSlots(cfg.sprites, cfg.charMap, cfg.sheet)
  const patch = spritePatch.readPatch(srcFile)
  if (!patch) return { pngBuf: slotsToSheetPng(gen, cfg.sheet), patchJson: null }

  const { slots, patch: surviving, warnings, changed } = spritePatch.applyPatch(gen, patch)
  for (const w of warnings) console.log(`⚠ sprite patch: ${w}`)
  const nSurv = Object.keys(surviving.slots).length
  if (nSurv > 0) console.log(`applied ${nSurv} hand-patched sprite slot(s) over the generator`)
  // rewrite the file only when it actually changed (pruned / re-anchored) so a
  // clean re-bake is git-silent; a fully-stale patch prunes to nothing → deleted.
  if (changed) spritePatch.writePatch(srcFile, surviving)
  return {
    pngBuf:   slotsToSheetPng(slots, cfg.sheet),
    patchJson: nSurv > 0 ? spritePatch.serializePatch(surviving) : null,
  }
}

// ── extract chunks from an existing cart ─────────────────────
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

// ── exports (so tools/play.js can reuse the build machinery) ──
// Box2D opt-in. A cart that #includes "box2d/box2d.h" links the vendored pure-C
// Box2D v3 (runtime/box2d/), built on demand into build/box2d/mac/libbox2d.a. Carts
// that don't include it pay nothing — box2d stays out of the default ~450-cart build.
// Returns the extra clang args (include path + static lib), or [] if not requested.
// See docs/design/box2d-integration.md.
function box2dArgs(source) {
  if (!source || !/box2d\/box2d\.h/.test(source)) return []
  const lib = path.join(BUILD_DIR, 'box2d', 'mac', 'libbox2d.a')
  if (!fs.existsSync(lib)) {
    process.stdout.write('(building box2d) ')
    require('child_process').execSync(`"${path.join(ROOT_DIR, 'tools', 'build-box2d.sh')}" --mac`, { stdio: 'pipe' })
  }
  return [`-I"${path.join(RUNTIME_DIR, 'box2d', 'include')}"`, `"${lib}"`]
}

module.exports = {
  ROOT_DIR, BUILD_DIR, RUNTIME_DIR, RAYLIB, CART_BIN,
  buildSpriteSheet, buildMap, makeBlankSpritePng, makePlaceholderPng,
  loadConfig, extractCartChunks, embedCartChunks, box2dArgs,
  // primitives reused by tools/sprite-preview.js (shared palette + encoder)
  makePng, makePngRGBA, parseSprite, PAL32, DEFAULT_CHAR_MAP,
  // Gap-2 sprite-patch machinery (make-cart is the bake-side consumer)
  genSlots, slotsToSheetPng, bakeSprites,
  // declared sheet size + the raw-atlas escape from the slot grid
  resolveSheet, atlasToSheetPng, sheetBufFor,
}

// After a bake, regenerate index.json so baking a cart auto-registers it (no manual
// index.json edit). A cart appears in the gallery IFF its source carries a de:meta block;
// warn loudly if it doesn't, since the bake otherwise silently won't show up.
function autoRegisterIndex(source) {
  if (!/\/\*\s*de:meta/.test(source)) {
    console.log('⚠ no de:meta block — this cart will NOT appear in the gallery.')
    console.log('  add a de:meta block to register it (see docs/design/cart-metadata.md).')
    return
  }
  try {
    const n = require('./build-cart-index.js').writeIndex()
    console.log(`index.json regenerated — ${n} carts`)
  } catch (e) {
    console.error('index.json regen failed: ' + e.message)
  }
}

// ── main ──────────────────────────────────────────────────────
if (require.main === module) runCli()

function runCli() {
const args = process.argv.slice(2)

if (args[0] === '--update') {
  // update the screenshot of an existing cart
  // usage: node make-cart.js --update <cart.png> <screenshot.png>
  const [, cartFile, screenshotFile] = args
  if (!cartFile || !screenshotFile) {
    console.error('usage: node tools/make-cart.js --update <cart.png> <screenshot.png>')
    process.exit(1)
  }
  const chunks = extractCartChunks(fs.readFileSync(cartFile))
  if (!chunks.source) { console.error('not a dreamengine cart'); process.exit(1) }
  const newPng = embedCartChunks(fs.readFileSync(screenshotFile), chunks)
  fs.writeFileSync(cartFile, newPng)
  console.log('updated screenshot in', cartFile)

} else if (args[0] === '--run') {
  // compile + run a cart in screenshot mode, then bake the result back in
  // usage: node tools/make-cart.js --run <cart.png>
  const { execSync, spawnSync } = require('child_process')
  const cartFile = args[1]
  if (!cartFile) { console.error('usage: node tools/make-cart.js --run <cart.png>'); process.exit(1) }

  const chunks = extractCartChunks(fs.readFileSync(cartFile))
  if (!chunks.source) { console.error('not a dreamengine cart'); process.exit(1) }

  // Bake into an ISOLATED per-cart scratch dir, never the shared build/ dir.
  // The live (libtcc) editor host watches build/cart.c by mtime and hot-swaps on
  // any change — so writing build/cart.c here would yank a user's running cart over
  // to THIS one. A per-cart dir also keeps parallel bakes of different carts from
  // colliding (the old "don't run --run in parallel" caveat). studio.c loads the
  // font + sprites from embedded headers, not cwd, so nothing else needs staging.
  const bakeName = (path.basename(cartFile).replace(/\.cart\.png$/i, '').replace(/[^a-z0-9_-]/gi, '_')) || 'cart'
  const BAKE_DIR = path.join(BUILD_DIR, '.bake', bakeName)
  const BAKE_BIN = path.join(BAKE_DIR, 'cart')
  fs.mkdirSync(BAKE_DIR, { recursive: true })

  // write source, sprites, map to the scratch dir
  fs.writeFileSync(path.join(BAKE_DIR, 'cart.c'), chunks.source)
  const spritesBuf = chunks.sprites
    ? Buffer.from(chunks.sprites.replace(/^data:image\/png;base64,/, ''), 'base64')
    : makeBlankSpritePng()
  fs.writeFileSync(path.join(BAKE_DIR, 'sprites.png'), spritesBuf)
  fs.writeFileSync(path.join(BAKE_DIR, 'map.dat'),
    chunks.map ? Buffer.from(chunks.map, 'base64') : Buffer.alloc(8192))

  // generate headers via xxd
  const xxd = (file) => execSync(`xxd -i ${file}`, { cwd: BAKE_DIR }).toString()
  fs.writeFileSync(path.join(BAKE_DIR, 'sprites_data.h'),
    xxd('sprites.png')
      .replace(/unsigned char sprites_png\[\]/, 'static const unsigned char SPRITES_DATA[]')
      .replace(/unsigned int sprites_png_len/,  'static const unsigned int  SPRITES_DATA_LEN'))
  fs.writeFileSync(path.join(BAKE_DIR, 'map_data.h'),
    xxd('map.dat')
      .replace(/unsigned char map_dat\[\]/, 'static const unsigned char MAP_DATA[]')
      .replace(/unsigned int map_dat_len/,  'static const unsigned int  MAP_DATA_LEN'))

  // bake the thumbnail at the cart's intended config so the screenshot matches
  // how it'll actually run. (SCALE only affects window size, not the captured
  // native-res canvas, so it stays 1 — the screenshot is identical either way.)
  let st = {}
  try { st = JSON.parse(chunks.settings || '{}') } catch {}
  const SW = st.screenW ?? 320, SH = st.screenH ?? 200
  const CW = st.cellW   ?? 16,  CH = st.cellH   ?? 16
  const MW = st.mapW    ?? 128, MH = st.mapH    ?? 64

  // compile
  const studioC = path.join(RUNTIME_DIR, 'studio.c')
  const cartSrc = path.join(BAKE_DIR, 'cart.c')
  const clangArgs = [
    `"${cartSrc}"`, `"${studioC}"`,
    `-I"${RUNTIME_DIR}"`, `-I"${BAKE_DIR}"`, `-I"${RAYLIB}/include"`,
    `-DSCREEN_W=${SW}`, `-DSCREEN_H=${SH}`, '-DSCALE=1',
    `-DMAP_W=${MW}`, `-DMAP_H=${MH}`, `-DCELL_W=${CW}`, `-DCELL_H=${CH}`,
    '-DTOUCH_CONTROLS_DEFAULT=0', '-Os', '-fno-delete-null-pointer-checks',
    ...box2dArgs(chunks.source),
    `"${RAYLIB}/lib/libraylib.a"`,
    '-framework OpenGL', '-framework Cocoa', '-framework IOKit',
    '-framework CoreVideo', '-framework CoreFoundation', '-framework CoreMIDI',
    '-framework AudioToolbox',   // mic_desktop.h AudioQueue capture (runtime/mic.h Tier-1 input)
    `-Wl,-dead_strip`, `-o "${BAKE_BIN}"`,
  ].join(' ')

  process.stdout.write('compiling... ')
  try {
    execSync(`clang ${clangArgs}`, { stdio: 'pipe' })
    console.log('ok')
  } catch (e) {
    console.error('failed\n' + (e.stderr?.toString() || e.message))
    process.exit(1)
  }

  // run with --screenshot: window opens briefly, 3 frames, exits, saves screenshot.png
  // darwin: run under `caffeinate -dims` — a SLEEPING DISPLAY segfaults raylib's window init
  // (even --headless/--screenshot), which silently killed unattended night bakes (bit 2026-07-02)
  console.log('running (window will flash briefly)...')
  // -u -t 1 first: -dims only PREVENTS display sleep, it cannot WAKE a display that is already off —
  // so an already-dark screen still segfaulted the bake (bit 2026-07-28, the gap the -dims fix left).
  if (process.platform === 'darwin') {
    spawnSync('caffeinate', ['-u', '-t', '1'], { stdio: 'ignore' })
    spawnSync('caffeinate', ['-dims', BAKE_BIN, '--screenshot'], { cwd: BAKE_DIR, stdio: 'inherit' })
  } else                             spawnSync(BAKE_BIN, ['--screenshot'], { cwd: BAKE_DIR, stdio: 'inherit' })

  // bake screenshot into cart
  const screenshotPath = path.join(BAKE_DIR, 'screenshot.png')
  if (!fs.existsSync(screenshotPath)) {
    console.error('screenshot.png not written — did the cart crash?')
    process.exit(1)
  }
  const newPng = embedCartChunks(fs.readFileSync(screenshotPath), chunks)
  fs.writeFileSync(cartFile, newPng)
  console.log('updated', cartFile)
  autoRegisterIndex(chunks.source)

} else {
  // create a new cart from source
  // usage: node make-cart.js <source.c> <output.cart.png>
  const [srcFile, outFile] = args
  if (!srcFile || !outFile) {
    console.error('usage: node tools/make-cart.js <source.c> <output.cart.png>')
    process.exit(1)
  }
  const source     = fs.readFileSync(srcFile, 'utf8')
  const cfg        = loadConfig(srcFile)
  const { pngBuf: spritesBuf, patchJson } = bakeSprites(srcFile, cfg)
  const mapBytes   = cfg.map ? buildMap(cfg.map.layout || cfg.map, cfg.map.tiles, cfg.mapW, cfg.mapH) : new Uint8Array(8192)
  const spritesUrl = 'data:image/png;base64,' + spritesBuf.toString('base64')
  const mapB64     = Buffer.from(mapBytes).toString('base64')
  // the config a cart is meant to run at — travels with the cart so loading it
  // restores the right screen/scale/cell/map dims regardless of editor globals.
  // mapW/mapH default to 128/64 to match buildMap()'s own defaults.
  const cartSettings = {
    screenW: cfg.screenW ?? 320, screenH: cfg.screenH ?? 200, scale: cfg.scale ?? 4,
    cellW:   cfg.cellW   ?? 16,  cellH:   cfg.cellH   ?? 16,
    mapW:    cfg.mapW    ?? 128,  mapH:    cfg.mapH    ?? 64,
    renderEvery: cfg.renderEvery ?? 1,   // present every Nth tick on web (heat lever); 1 = every tick
  }
  // Preserve the existing thumbnail image when re-embedding source into a cart
  // that's already been baked — only fall back to the placeholder for a brand
  // new cart. Otherwise a plain `make-cart.js <src> <png>` (which doesn't run
  // the cart) would clobber a real screenshot with a blank placeholder, and
  // it's easy to forget the follow-up `--run` that re-bakes it. embedCartChunks
  // strips the old de:* chunks, so re-using the baked PNG as the base is clean.
  const reuse      = fs.existsSync(outFile)
  const baseImage  = reuse ? fs.readFileSync(outFile) : makePlaceholderPng()
  // mirror the surviving sprite patch into the .cart.png (de:spritepatch) so the
  // editor loads it alongside de:source; embedCartChunks drops any stale de:* first.
  const chunks     = { source, sprites: spritesUrl, map: mapB64, settings: JSON.stringify(cartSettings) }
  if (patchJson) chunks.spritepatch = patchJson
  const cartPng    = embedCartChunks(baseImage, chunks)
  fs.writeFileSync(outFile, cartPng)
  console.log(reuse
    ? `re-embedded source into ${outFile} (kept existing thumbnail — run \`--run\` to re-bake if visuals changed)`
    : `wrote ${outFile} (placeholder thumbnail — run \`--run\` to bake a real screenshot)`)
  autoRegisterIndex(source)
}
}
