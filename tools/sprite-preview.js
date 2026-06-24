#!/usr/bin/env node
'use strict'
// sprite-preview.js — render a cart's PROGRAMMATIC sprites (the ones a <cart>.cart.js
// builds with sprite-draw.js) to a single labelled PNG, WITHOUT compiling or running
// the cart. The tight feedback loop for code-drawn sprites: tweak the .cart.js, run
// this, Read the PNG, repeat in seconds — instead of bake → --run → dig out sprites.png.
//
// Usage:
//   node tools/sprite-preview.js <cart>            # by name → tools/carts/<cart>.cart.js
//   node tools/sprite-preview.js path/to/x.cart.js # or an explicit .cart.js / .c path
//   node tools/sprite-preview.js boom --scale 10 --cols 6 --out /tmp/boom-icons.png
//   node tools/sprite-preview.js boom --slots 0-5,28,29   # only these slots
//
// Options:
//   --scale N   pixels per sprite-pixel (default 8 → a 16px sprite drawn at 128px)
//   --cols  N   sprites per row (default 8, the sheet width)
//   --slots L   comma list / a-b ranges of slot numbers to include (default: all used)
//   --out   F   output path (default build/sprite-preview.png)
//
// Each sprite sits on a checkerboard so index-0 (the colorkey transparent) reads as
// transparent, with its slot number printed underneath. Reuses make-cart.js's palette
// + PNG encoder + sprite parser, so the preview is pixel-identical to the baked sheet.

const fs   = require('fs')
const path = require('path')
const { makePng, parseSprite, PAL32, DEFAULT_CHAR_MAP } = require('./make-cart.js')

const SP = 16  // sprite slots are always 16×16

// ── tiny 3×5 digit font for the slot-number labels ───────────────────────────
const DIGITS = {
  '0': ['111','101','101','101','111'], '1': ['010','110','010','010','111'],
  '2': ['111','001','111','100','111'], '3': ['111','001','111','001','111'],
  '4': ['101','101','111','001','001'], '5': ['111','100','111','001','111'],
  '6': ['111','100','111','101','111'], '7': ['111','001','010','010','010'],
  '8': ['111','101','111','101','111'], '9': ['111','101','111','001','111'],
}

function parseArgs(argv) {
  const o = { scale: 8, cols: 8, out: null, slots: null }
  const rest = []
  for (let i = 0; i < argv.length; i++) {
    const a = argv[i]
    if (a === '--scale') o.scale = Math.max(1, parseInt(argv[++i]) || 8)
    else if (a === '--cols') o.cols = Math.max(1, parseInt(argv[++i]) || 8)
    else if (a === '--out') o.out = argv[++i]
    else if (a === '--slots') o.slots = argv[++i]
    else rest.push(a)
  }
  o.target = rest[0]
  return o
}

// "0-5,28,29" → Set{0,1,2,3,4,5,28,29}
function parseSlotFilter(spec) {
  const set = new Set()
  for (const part of spec.split(',')) {
    const m = part.match(/^(\d+)-(\d+)$/)
    if (m) { for (let i = +m[1]; i <= +m[2]; i++) set.add(i) }
    else if (/^\d+$/.test(part)) set.add(+part)
  }
  return set
}

function resolveConfigPath(target) {
  if (!target) return null
  if (target.endsWith('.cart.js')) return path.resolve(target)
  if (target.endsWith('.c'))       return path.resolve(target.replace(/\.c$/, '.cart.js'))
  return path.resolve('tools/carts', target.replace(/\.cart\.png$/, '') + '.cart.js')
}

function main() {
  const o = parseArgs(process.argv.slice(2))
  if (!o.target) {
    console.error('usage: node tools/sprite-preview.js <cart|path.cart.js> [--scale N] [--cols N] [--slots L] [--out F]')
    process.exit(1)
  }
  const cfgPath = resolveConfigPath(o.target)
  if (!fs.existsSync(cfgPath)) {
    console.error(`no .cart.js at ${cfgPath}\n  → this cart has no programmatic sprites (it draws everything in C, or hand-paints in the sprite editor).`)
    process.exit(1)
  }

  let cfg
  try { cfg = require(cfgPath) }
  catch (e) { console.error(`failed to load ${cfgPath}:\n  ${e.message}`); process.exit(1) }

  const sprites = cfg.sprites
  if (!sprites || !Object.keys(sprites).length) {
    console.error(`${path.basename(cfgPath)} has no .sprites — nothing to preview.`); process.exit(1)
  }

  const map = cfg.charMap ? { ...DEFAULT_CHAR_MAP, ...cfg.charMap } : DEFAULT_CHAR_MAP
  let slots = Object.keys(sprites).map(Number).sort((a, b) => a - b)
  if (o.slots) {
    const want = parseSlotFilter(o.slots)
    slots = slots.filter((s) => want.has(s))
    if (!slots.length) { console.error(`no sprites match --slots ${o.slots}`); process.exit(1) }
  }

  // parse each slot to a 256-index tile (warn on odd sizes, then pad/truncate)
  const tiles = slots.map((s) => {
    let px = parseSprite(sprites[s], map)
    if (px.length !== SP * SP) {
      console.warn(`  slot ${s}: expected ${SP * SP} pixels, got ${px.length} (padded/truncated — use split() for multi-slot sprites)`)
      px = px.slice(0, SP * SP)
      while (px.length < SP * SP) px.push(0)
    }
    return px
  })

  // ── layout ──
  const scale  = o.scale
  const cols   = Math.min(o.cols, slots.length)
  const ds     = Math.max(1, Math.round(scale / 4))   // digit pixel size
  const labelH = 5 * ds + 2
  const pad    = Math.max(4, scale)
  const cellW  = SP * scale + pad
  const cellH  = SP * scale + labelH + pad
  const rows   = Math.ceil(slots.length / cols)
  const W = cols * cellW + pad
  const H = rows * cellH + pad

  // RGB framebuffer, prefilled with a checkerboard (so transparency reads)
  const checkA = [38, 38, 46], checkB = [56, 56, 66], cz = Math.max(3, Math.floor(scale / 2))
  const img = new Array(H)
  for (let y = 0; y < H; y++) {
    img[y] = new Array(W)
    for (let x = 0; x < W; x++)
      img[y][x] = ((((x / cz) | 0) + ((y / cz) | 0)) & 1) ? checkA : checkB
  }
  const put = (x, y, c) => { if (x >= 0 && x < W && y >= 0 && y < H) img[y][x] = c }
  const block = (x, y, w, h, c) => { for (let j = 0; j < h; j++) for (let i = 0; i < w; i++) put(x + i, y + j, c) }
  const frame = (x, y, w, h, c) => {
    for (let i = 0; i < w; i++) { put(x + i, y, c); put(x + i, y + h - 1, c) }
    for (let j = 0; j < h; j++) { put(x, y + j, c); put(x + w - 1, y + j, c) }
  }
  function drawLabel(str, cx, top, c) {
    const gw = 3 * ds + ds, tw = str.length * gw - ds
    let x = cx - (tw >> 1)
    for (const ch of str) {
      const g = DIGITS[ch]; if (g) for (let r = 0; r < 5; r++) for (let col = 0; col < 3; col++)
        if (g[r][col] === '1') block(x + col * ds, top + r * ds, ds, ds, c)
      x += gw
    }
  }

  // ── draw each sprite cell ──
  const GRID = [70, 70, 82], WHITE = [255, 241, 232]
  tiles.forEach((px, k) => {
    const cx0 = pad + (k % cols) * cellW
    const cy0 = pad + ((k / cols) | 0) * cellH
    for (let sy = 0; sy < SP; sy++)
      for (let sx = 0; sx < SP; sx++) {
        const idx = px[sy * SP + sx]
        if (idx === 0) continue                         // transparent → checker shows through
        block(cx0 + sx * scale, cy0 + sy * scale, scale, scale, PAL32[idx] || [255, 0, 255])
      }
    frame(cx0 - 1, cy0 - 1, SP * scale + 2, SP * scale + 2, GRID)
    drawLabel(String(slots[k]), cx0 + (SP * scale >> 1), cy0 + SP * scale + 2, WHITE)
  })

  const out = o.out || path.join('build', 'sprite-preview.png')
  fs.mkdirSync(path.dirname(path.resolve(out)), { recursive: true })
  fs.writeFileSync(out, makePng(W, H, (x, y) => img[y][x]))
  console.log(`${path.basename(cfgPath)}: ${slots.length} sprite${slots.length === 1 ? '' : 's'} (slots ${slots.join(', ')})`)
  console.log(`→ ${out}   (${cols}×${rows} grid, ${W}×${H}px, scale ${scale})`)
}

main()
