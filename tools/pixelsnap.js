#!/usr/bin/env node
// pixelsnap.js — clean up "AI pixel art": snap soft, off-grid pixels back onto a
// real grid and posterize the gradient-soup down to a small palette.
//
// AI image generators produce art with a pixel-art *look* but none of the
// constraints: anti-aliased edges, pixels that don't line up on any grid, and
// hundreds of colors from baked-in shading. This tool reverses that:
//
//   1. DECODE   any image (PNG/JPG/webp/…) to raw RGBA via sharp.
//   2. SNAP     detect the underlying pixel cell size (or take --grid/--cell),
//               then collapse each cell to ONE color (median = robust to soft
//               edges). Output is now genuinely 1 logical pixel per cell.
//   3. POSTERIZE reduce to a small palette — either derived from the image
//               (median-cut, --colors N) or a named/loaded palette (--palette).
//   4. ENCODE   write a clean PNG at logical size, or --scale N blown up
//               nearest-neighbor for crisp viewing. Alpha is thresholded 1-bit.
//
// SAFETY: never writes to the input path. Default output is "<input>.snapped.png"
// alongside the source; an explicit output equal to the input is refused.
//
// Usage:
//   node tools/pixelsnap.js <input> [output.png] [options]
//
// Options:
//   --grid WxH       snap to this logical resolution (e.g. 96x96). Overrides auto-detect.
//   --cell N         source pixels per logical pixel (alt to --grid).
//   --colors N       palette size via median-cut derived from the image (default 16).
//   --palette NAME   map to a named palette instead of deriving. NAME is a file in
//                    editor/public/palettes/ (e.g. "pico32"), or a path to a
//                    .json {"palette":[...]} / a newline list of hex colors.
//   --two A,B        force exactly TWO colors and Floyd–Steinberg dither between them
//                    (the engine's 1-bit ink/paper look). Each color is #rrggbb or a
//                    pico index 0..31, e.g. --two 0,7  or  --two #1d2b53,#fff1e8.
//   --sample MODE    per-cell color: median (default) | mean | mode.
//   --scale N        upscale output N× nearest-neighbor for viewing (default 1).
//   --dither [MODE]  dither when mapping to the palette (default off). MODE:
//                    fs (Floyd–Steinberg, organic/noisy — the default if MODE
//                    omitted), bayer (ordered 4×4), bayer8 (ordered 8×8), or
//                    diag (diagonal ordered, like the dpaint cart). Ordered
//                    modes keep flat areas solid and pattern only the in-between
//                    tones, so a full palette (e.g. --palette pico32) stays clean.
//   --clean [N]      remove speckle: recolor color islands of ≤N cells into their
//                    surroundings after snapping (default N=1). Cleans pinholes too.
//   --alpha N        alpha cutoff 0..255 for 1-bit transparency (default 128).
//   --no-alpha       ignore source alpha (treat everything opaque).
//   -h, --help
//
// Color matching is perceptual (OKLab), so palette mapping respects how colors
// actually look, not raw RGB distance.
//
// Examples:
//   node tools/pixelsnap.js docs/marketing/tinyjam/icons/tinyjam-icon-pixel.png
//   node tools/pixelsnap.js in.png out.png --grid 96x96 --colors 24 --scale 6
//   node tools/pixelsnap.js in.png --palette pico32 --dither --scale 8
//
// MAKING AN APP ICON with this? The square you snap is NOT what iOS shows: it masks
// the icon to a squircle and throws ~6% away, corners first. Draw against the mask
// (tools/icon-mask.js template --overlay), then check the snapped result:
//   node tools/icon-mask.js check   <icon.png>    # per corner: background, or lost detail?
//   node tools/icon-mask.js preview <icon.png>    # how it reads at every real size
// Grid choice interacts with the mask: a coarse --grid puts big cells in the corners,
// so a cut lands on whole cells and reads as a chewed edge. docs/design/app-icon-mask.md

'use strict'

const fs   = require('fs')
const path = require('path')
const sharp = require('sharp')

const ROOT_DIR    = path.join(__dirname, '..')
const PALETTE_DIR = path.join(ROOT_DIR, 'editor', 'public', 'palettes')

// ── arg parsing ─────────────────────────────────────────────────────────────
function parseArgs(argv) {
  const opts = {
    colors: 16, sample: 'median', scale: 1, dither: false,
    alpha: 128, useAlpha: true, clean: 0,
    grid: null, cell: null, palette: null, two: null,
  }
  const positional = []
  for (let i = 0; i < argv.length; i++) {
    const a = argv[i]
    const next = () => argv[++i]
    switch (a) {
      case '-h': case '--help': opts.help = true; break
      case '--grid': {
        const m = /^(\d+)x(\d+)$/.exec(next())
        if (!m) die('--grid wants WxH, e.g. 96x96')
        opts.grid = [parseInt(m[1], 10), parseInt(m[2], 10)]; break
      }
      case '--cell':    opts.cell = parseFloat(next()); break
      case '--colors':  opts.colors = parseInt(next(), 10); break
      case '--palette': opts.palette = next(); break
      case '--two':     opts.two = next(); break
      case '--sample':  opts.sample = next(); break
      case '--scale':   opts.scale = parseInt(next(), 10); break
      case '--dither': {
        const peek = argv[i + 1]
        opts.dither = (peek && DITHER_MODES.includes(peek)) ? next() : 'fs'
        break
      }
      case '--clean': {
        const peek = argv[i + 1]
        opts.clean = (peek && /^\d+$/.test(peek)) ? parseInt(next(), 10) : 1
        break
      }
      case '--alpha':   opts.alpha = parseInt(next(), 10); break
      case '--no-alpha': opts.useAlpha = false; break
      default:
        if (a.startsWith('--')) die(`unknown option: ${a}`)
        positional.push(a)
    }
  }
  opts.input = positional[0]
  opts.output = positional[1]
  if (!['median', 'mean', 'mode'].includes(opts.sample)) die(`--sample must be median|mean|mode`)
  return opts
}

function die(msg) { console.error(`pixelsnap: ${msg}`); process.exit(1) }

const DITHER_MODES = ['fs', 'bayer', 'bayer8', 'diag']

// The leading contiguous comment block (after the shebang) IS the help text.
const HELP = (() => {
  const out = []
  for (const l of fs.readFileSync(__filename, 'utf8').split('\n')) {
    if (l.startsWith('//')) out.push(l.replace(/^\/\/ ?/, ''))
    else if (out.length) break
  }
  return out.join('\n')
})()

// ── palette file loading ────────────────────────────────────────────────────
function hexToRgb(h) {
  const s = h.trim().replace(/^#/, '')
  return [parseInt(s.slice(0, 2), 16), parseInt(s.slice(2, 4), 16), parseInt(s.slice(4, 6), 16)]
}

function loadPalette(name) {
  // Resolve: a path with a slash / .json, else a named palette in PALETTE_DIR.
  let file = name
  if (!name.includes('/') && !name.endsWith('.json') && !name.endsWith('.txt') && !name.endsWith('.hex')) {
    file = path.join(PALETTE_DIR, `${name}.json`)
  }
  if (!fs.existsSync(file)) die(`palette not found: ${name} (looked at ${file})`)
  const raw = fs.readFileSync(file, 'utf8')
  let hexes
  if (file.endsWith('.json')) {
    const j = JSON.parse(raw)
    hexes = Array.isArray(j) ? j : j.palette
    if (!Array.isArray(hexes)) die(`palette json needs an array or {"palette":[...]}`)
  } else {
    hexes = raw.split(/\s+/).filter(Boolean)
  }
  return hexes.map(hexToRgb)
}

// A single color from the CLI: "#rrggbb" / "rrggbb", or an index into pico32.
function parseColor(token) {
  const t = token.trim()
  if (/^#?[0-9a-fA-F]{6}$/.test(t)) return hexToRgb(t)
  if (/^\d+$/.test(t)) {
    const pal = loadPalette('pico32'), idx = parseInt(t, 10)
    if (idx < 0 || idx >= pal.length) die(`color index ${idx} out of range for pico32 (0..${pal.length - 1})`)
    return pal[idx]
  }
  die(`bad color "${token}" — use #rrggbb or a pico index 0..31`)
}

// ── grid detection ──────────────────────────────────────────────────────────
// Edge energy along one axis = summed abs luminance step between adjacent
// lines, counting only spans where both samples are opaque. A real pixel grid
// concentrates that energy at regular spacing; we find the cell size whose comb
// of grid-lines captures the most energy above a uniform baseline.
function luminance(r, g, b) { return 0.299 * r + 0.587 * g + 0.114 * b }

function edgeEnergyCols(data, W, H, ch, alphaCut, useAlpha) {
  const E = new Float64Array(W)
  for (let x = 1; x < W; x++) {
    let sum = 0
    for (let y = 0; y < H; y++) {
      const i = (y * W + x) * ch, j = (y * W + (x - 1)) * ch
      if (useAlpha && (data[i + 3] < alphaCut || data[j + 3] < alphaCut)) continue
      sum += Math.abs(luminance(data[i], data[i + 1], data[i + 2]) -
                      luminance(data[j], data[j + 1], data[j + 2]))
    }
    E[x] = sum
  }
  return E
}

function edgeEnergyRows(data, W, H, ch, alphaCut, useAlpha) {
  const E = new Float64Array(H)
  for (let y = 1; y < H; y++) {
    let sum = 0
    for (let x = 0; x < W; x++) {
      const i = (y * W + x) * ch, j = ((y - 1) * W + x) * ch
      if (useAlpha && (data[i + 3] < alphaCut || data[j + 3] < alphaCut)) continue
      sum += Math.abs(luminance(data[i], data[i + 1], data[i + 2]) -
                      luminance(data[j], data[j + 1], data[j + 2]))
    }
    E[y] = sum
  }
  return E
}

// Box-smooth a signal — collapses anti-aliased edge ramps (which spread one
// real boundary across 2–3 px) back onto a single line so the comb can find it.
function smooth(E, r) {
  if (!r) return E
  const L = E.length, O = new Float64Array(L)
  for (let i = 0; i < L; i++) {
    let s = 0, n = 0
    for (let k = -r; k <= r; k++) { const j = i + k; if (j >= 0 && j < L) { s += E[j]; n++ } }
    O[i] = s / n
  }
  return O
}

// Best cell size + a confidence score for the edge comb. Returns {cell, score}.
// score is "fraction of edge energy a single grid phase captures, above the
// uniform baseline" — high means a real grid, near-0 means no grid (the usual
// case for AI art, where cell sizes wander). Caller decides whether to trust it.
function detectCell(E) {
  const Es = smooth(E, 1)
  const L = Es.length
  let total = 0
  for (let i = 0; i < L; i++) total += Es[i]
  if (total === 0) return { cell: null, score: 0 }
  const minP = 6, maxP = Math.max(minP, Math.floor(L / 8))
  let bestP = minP, bestScore = -Infinity
  for (let p = minP; p <= maxP; p++) {
    const bucket = new Float64Array(p)
    for (let x = 0; x < L; x++) bucket[x % p] += Es[x]
    let combMax = 0
    for (let b = 0; b < p; b++) if (bucket[b] > combMax) combMax = bucket[b]
    const score = combMax / total - 1 / p
    if (score > bestScore) { bestScore = score; bestP = p }
  }
  return { cell: bestP, score: bestScore }
}

// ── per-cell color reduction ────────────────────────────────────────────────
function reduceCell(pixels, mode) {
  // pixels: array of [r,g,b]
  const n = pixels.length
  if (n === 0) return [0, 0, 0]
  if (mode === 'mean') {
    let r = 0, g = 0, b = 0
    for (const p of pixels) { r += p[0]; g += p[1]; b += p[2] }
    return [Math.round(r / n), Math.round(g / n), Math.round(b / n)]
  }
  if (mode === 'mode') {
    // Most common color after coarse 5-bit/channel quantization, returned as
    // the average of the winning bin's true colors (so it stays accurate).
    const bins = new Map()
    for (const p of pixels) {
      const key = ((p[0] >> 3) << 10) | ((p[1] >> 3) << 5) | (p[2] >> 3)
      let e = bins.get(key); if (!e) { e = [0, 0, 0, 0]; bins.set(key, e) }
      e[0] += p[0]; e[1] += p[1]; e[2] += p[2]; e[3]++
    }
    let best = null
    for (const e of bins.values()) if (!best || e[3] > best[3]) best = e
    return [Math.round(best[0] / best[3]), Math.round(best[1] / best[3]), Math.round(best[2] / best[3])]
  }
  // median: per-channel median is robust to anti-aliased edge fringes.
  const med = ch => {
    const arr = pixels.map(p => p[ch]).sort((a, b) => a - b)
    return arr[arr.length >> 1]
  }
  return [med(0), med(1), med(2)]
}

// ── median-cut palette from a set of colors ───────────────────────────────────
function medianCut(colors, k) {
  if (colors.length === 0) return [[0, 0, 0]]
  let boxes = [colors.slice()]
  const rangeOf = box => {
    const mn = [255, 255, 255], mx = [0, 0, 0]
    for (const c of box) for (let i = 0; i < 3; i++) {
      if (c[i] < mn[i]) mn[i] = c[i]
      if (c[i] > mx[i]) mx[i] = c[i]
    }
    return [mx[0] - mn[0], mx[1] - mn[1], mx[2] - mn[2]]
  }
  while (boxes.length < k) {
    // Split the box with the widest single-channel spread.
    let bi = -1, bAxis = 0, bSpread = -1
    for (let i = 0; i < boxes.length; i++) {
      if (boxes[i].length < 2) continue
      const r = rangeOf(boxes[i])
      const axis = r[0] >= r[1] && r[0] >= r[2] ? 0 : (r[1] >= r[2] ? 1 : 2)
      if (r[axis] > bSpread) { bSpread = r[axis]; bi = i; bAxis = axis }
    }
    if (bi < 0) break  // nothing left to split
    const box = boxes[bi].slice().sort((a, b) => a[bAxis] - b[bAxis])
    const mid = box.length >> 1
    boxes.splice(bi, 1, box.slice(0, mid), box.slice(mid))
  }
  return boxes.map(box => {
    let r = 0, g = 0, b = 0
    for (const c of box) { r += c[0]; g += c[1]; b += c[2] }
    return [Math.round(r / box.length), Math.round(g / box.length), Math.round(b / box.length)]
  })
}

// ── perceptual color distance (OKLab) ─────────────────────────────────────────
// Nearest-palette matching in OKLab beats raw RGB: it respects how colors
// actually look, so pastels/skin tones map to the right entry instead of a
// numerically-close but visibly-wrong one.
function rgb2oklab(r, g, b) {
  const lin = c => { c /= 255; return c <= 0.04045 ? c / 12.92 : Math.pow((c + 0.055) / 1.055, 2.4) }
  const R = lin(r), G = lin(g), B = lin(b)
  const l = Math.cbrt(0.4122214708 * R + 0.5363325363 * G + 0.0514459929 * B)
  const m = Math.cbrt(0.2119034982 * R + 0.6806995451 * G + 0.1073969566 * B)
  const s = Math.cbrt(0.0883024619 * R + 0.2817188376 * G + 0.6299787005 * B)
  return [
    0.2104542553 * l + 0.7936177850 * m - 0.0040720468 * s,
    1.9779984951 * l - 2.4285922050 * m + 0.4505937099 * s,
    0.0259040371 * l + 0.7827717662 * m - 0.8086757660 * s,
  ]
}

const _labCache = new WeakMap()  // palette array → its OKLab coords (computed once)
function nearest(palette, r, g, b) {
  let lab = _labCache.get(palette)
  if (!lab) { lab = palette.map(p => rgb2oklab(p[0], p[1], p[2])); _labCache.set(palette, lab) }
  const t = rgb2oklab(r, g, b)
  let bi = 0, bd = Infinity
  for (let i = 0; i < lab.length; i++) {
    const dL = t[0] - lab[i][0], dA = t[1] - lab[i][1], dB = t[2] - lab[i][2]
    const d = dL * dL + dA * dA + dB * dB
    if (d < bd) { bd = d; bi = i }
  }
  return bi
}

// ── ordered (structured) dithering ────────────────────────────────────────────
// Floyd–Steinberg diffuses error and looks organic/noisy. Ordered dithering
// lays a fixed REPEATING threshold pattern instead — flat regions that already
// land on a palette color stay solid, and only in-between tones break into the
// pattern. Matches the dpaint cart's 4×4 Bayer gradient + diagonal fill look.
const BAYER4 = [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]  // dpaint's matrix
const BAYER8 = [
  [0, 32, 8, 40, 2, 34, 10, 42], [48, 16, 56, 24, 50, 18, 58, 26],
  [12, 44, 4, 36, 14, 46, 6, 38], [60, 28, 52, 20, 62, 30, 54, 22],
  [3, 35, 11, 43, 1, 33, 9, 41], [51, 19, 59, 27, 49, 17, 57, 25],
  [15, 47, 7, 39, 13, 45, 5, 37], [63, 31, 55, 23, 61, 29, 53, 21],
]
// Threshold in [0,1) for cell (x,y). 'diag' indexes Bayer4 by diagonal coords,
// rotating the lattice 45° so the texture runs diagonally like dpaint's 0x8421.
function threshold(mode, x, y) {
  if (mode === 'bayer8') return (BAYER8[y & 7][x & 7] + 0.5) / 64
  if (mode === 'diag')   return (BAYER4[(x + y) & 3][(x - y) & 3] + 0.5) / 16
  return (BAYER4[y & 3][x & 3] + 0.5) / 16   // 'bayer'
}

// Map each cell to the palette using an ordered threshold pattern. For a cell
// color C we take its nearest palette entry c1 and the entry bracketing it from
// the far side (nearest to the reflection 2C−c1); the cell shows c2 instead of
// c1 when C sits far enough toward c2 that the pattern threshold trips. Result:
// the full palette is preserved, dither appears only where C lands between two
// colors — never the FS speckle in already-flat areas.
function orderedDither(cellRGB, palette, GW, GH, mode) {
  const clamp = v => v < 0 ? 0 : v > 255 ? 255 : v
  const final = new Array(GW * GH)
  for (let y = 0; y < GH; y++) for (let x = 0; x < GW; x++) {
    const idx = y * GW + x, c = cellRGB[idx]
    if (!c) { final[idx] = null; continue }
    const c1 = palette[nearest(palette, c[0], c[1], c[2])]
    const c2 = palette[nearest(palette, clamp(2 * c[0] - c1[0]), clamp(2 * c[1] - c1[1]), clamp(2 * c[2] - c1[2]))]
    if (c2 === c1) { final[idx] = c1; continue }
    const vx = c2[0] - c1[0], vy = c2[1] - c1[1], vz = c2[2] - c1[2]
    const denom = vx * vx + vy * vy + vz * vz
    let t = denom ? ((c[0] - c1[0]) * vx + (c[1] - c1[1]) * vy + (c[2] - c1[2]) * vz) / denom : 0
    t = t < 0 ? 0 : t > 1 ? 1 : t
    final[idx] = t > threshold(mode, x, y) ? c2 : c1
  }
  return final
}

// ── speckle cleanup ───────────────────────────────────────────────────────────
// Snapping AI art leaves stray 1-px islands and pinholes. Find connected same-
// color regions (4-conn, transparency is its own "color"); any region of ≤max
// cells is recolored to the most common color bordering it. One pass, computed
// against the original grid so neighbors don't cascade within the pass.
function cleanIslands(final, GW, GH, max) {
  const N = GW * GH
  const tok = new Int32Array(N)
  for (let i = 0; i < N; i++) { const c = final[i]; tok[i] = c ? (c[0] << 16 | c[1] << 8 | c[2]) : -1 }
  const comp = new Int32Array(N).fill(-1)
  const comps = [], stack = []
  for (let s = 0; s < N; s++) {
    if (comp[s] !== -1) continue
    const t = tok[s], cells = []; comp[s] = comps.length; stack.length = 0; stack.push(s)
    while (stack.length) {
      const i = stack.pop(); cells.push(i)
      const x = i % GW, y = (i / GW) | 0
      const nb = []
      if (x > 0) nb.push(i - 1); if (x + 1 < GW) nb.push(i + 1)
      if (y > 0) nb.push(i - GW); if (y + 1 < GH) nb.push(i + GW)
      for (const j of nb) if (comp[j] === -1 && tok[j] === t) { comp[j] = comps.length; stack.push(j) }
    }
    comps.push({ cells, tok: t })
  }
  const repl = []
  for (const cmp of comps) {
    if (cmp.cells.length > max) continue
    const tally = new Map()
    for (const i of cmp.cells) {
      const x = i % GW, y = (i / GW) | 0
      const nb = []
      if (x > 0) nb.push(i - 1); if (x + 1 < GW) nb.push(i + 1)
      if (y > 0) nb.push(i - GW); if (y + 1 < GH) nb.push(i + GW)
      for (const j of nb) if (comp[j] !== comp[i]) tally.set(tok[j], (tally.get(tok[j]) || 0) + 1)
    }
    let bestTok = null, bestN = -1
    for (const [k, n] of tally) if (n > bestN) { bestN = n; bestTok = k }
    if (bestTok != null && bestTok !== cmp.tok) repl.push({ cells: cmp.cells, tok: bestTok })
  }
  for (const r of repl) {
    const c = r.tok === -1 ? null : [(r.tok >> 16) & 255, (r.tok >> 8) & 255, r.tok & 255]
    for (const i of r.cells) final[i] = c
  }
  return repl.reduce((n, r) => n + r.cells.length, 0)
}

// ── main ──────────────────────────────────────────────────────────────────────
async function main() {
  const opts = parseArgs(process.argv.slice(2))
  if (opts.help || !opts.input) { console.log(HELP); process.exit(opts.help ? 0 : 1) }
  if (!fs.existsSync(opts.input)) die(`input not found: ${opts.input}`)

  // Output path — never the input.
  const inAbs = fs.realpathSync(opts.input)
  let output = opts.output
  if (!output) {
    const ext = path.extname(opts.input)
    output = path.join(path.dirname(opts.input), path.basename(opts.input, ext) + '.snapped.png')
  }
  const outAbs = path.resolve(output)
  if (outAbs === inAbs || (fs.existsSync(output) && fs.realpathSync(output) === inAbs)) {
    die(`refusing to overwrite the input image (${opts.input}). Pick a different output path.`)
  }

  // Decode → raw RGBA.
  const img = sharp(opts.input).ensureAlpha()
  const { data, info } = await img.raw().toBuffer({ resolveWithObject: true })
  const W = info.width, H = info.height, ch = info.channels
  const alphaCut = opts.useAlpha ? opts.alpha : -1

  // Determine the logical grid.
  let GW, GH, autoNote = null
  if (opts.grid) {
    [GW, GH] = opts.grid
  } else if (opts.cell) {
    GW = Math.max(1, Math.round(W / opts.cell))
    GH = Math.max(1, Math.round(H / opts.cell))
  } else {
    // Auto: detect a cell size per axis, then force SQUARE pixels (legit pixel
    // art is square) by taking the coarser of the two — finer guesses are the
    // risky ones. Clamp the logical resolution to a sane band, and if the grid
    // signal is weak (typical for AI art) fall back to ~96 px on the long edge.
    const col = detectCell(edgeEnergyCols(data, W, H, ch, opts.alpha, opts.useAlpha))
    const row = detectCell(edgeEnergyRows(data, W, H, ch, opts.alpha, opts.useAlpha))
    const conf = Math.min(col.score, row.score)
    let cell
    if (conf < 0.08 || !col.cell || !row.cell) {
      cell = Math.max(W, H) / 96               // weak signal → sensible default
      autoNote = `weak grid signal (confidence ${conf.toFixed(2)}) — guessed; pass --grid WxH or --cell N to refine`
    } else {
      cell = Math.max(col.cell, row.cell)      // square, coarser = safer
      autoNote = `confidence ${conf.toFixed(2)}`
    }
    GW = Math.min(256, Math.max(8, Math.round(W / cell)))
    GH = Math.min(256, Math.max(8, Math.round(H / cell)))
  }

  // Collapse each cell → one color + opacity.
  const cellRGB = new Array(GW * GH)      // [r,g,b] or null (transparent)
  const opaqueColors = []
  for (let gy = 0; gy < GH; gy++) {
    const y0 = Math.floor(gy * H / GH), y1 = Math.max(y0 + 1, Math.floor((gy + 1) * H / GH))
    for (let gx = 0; gx < GW; gx++) {
      const x0 = Math.floor(gx * W / GW), x1 = Math.max(x0 + 1, Math.floor((gx + 1) * W / GW))
      const px = []
      let opaque = 0, count = 0
      for (let y = y0; y < y1; y++) for (let x = x0; x < x1; x++) {
        const i = (y * W + x) * ch
        count++
        if (opts.useAlpha && data[i + 3] < opts.alpha) continue
        opaque++
        px.push([data[i], data[i + 1], data[i + 2]])
      }
      const idx = gy * GW + gx
      if (opts.useAlpha && opaque < count / 2) { cellRGB[idx] = null; continue }
      const c = reduceCell(px.length ? px : [[0, 0, 0]], opts.sample)
      cellRGB[idx] = c
      opaqueColors.push(c)
    }
  }

  // Build the palette.
  let palette, paletteSrc
  if (opts.two) {
    const parts = opts.two.split(',').map(s => s.trim()).filter(Boolean)
    if (parts.length !== 2) die('--two wants exactly two colors, e.g. --two 0,7 or --two #1d2b53,#fff1e8')
    palette = parts.map(parseColor)
    if (!opts.dither) opts.dither = 'fs'      // the whole point of 2-color is the dither
    paletteSrc = `2-color (${parts.join(' / ')})`
  } else if (opts.palette) {
    palette = loadPalette(opts.palette)
    paletteSrc = opts.palette
  } else {
    palette = medianCut(opaqueColors, opts.colors)
    paletteSrc = `median-cut (${palette.length})`
  }

  // Map cells → palette colors. final[idx] = [r,g,b] or null (transparent).
  let final
  if (opts.dither === 'fs') {
    // Floyd–Steinberg: organic, error-diffused. Best for photographic gradients.
    final = new Array(GW * GH)
    const work = cellRGB.map(c => c ? [c[0], c[1], c[2]] : null)  // float error buffer
    const spread = (idx, er, eg, eb, f) => {
      const c = work[idx]; if (!c) return
      c[0] += er * f; c[1] += eg * f; c[2] += eb * f
    }
    for (let gy = 0; gy < GH; gy++) for (let gx = 0; gx < GW; gx++) {
      const idx = gy * GW + gx
      const c = work[idx]
      if (!c) { final[idx] = null; continue }
      const p = palette[nearest(palette, c[0], c[1], c[2])]
      const er = c[0] - p[0], eg = c[1] - p[1], eb = c[2] - p[2]   // error diffused in RGB
      if (gx + 1 < GW)                spread(idx + 1,      er, eg, eb, 7 / 16)
      if (gy + 1 < GH && gx > 0)      spread(idx + GW - 1, er, eg, eb, 3 / 16)
      if (gy + 1 < GH)                spread(idx + GW,     er, eg, eb, 5 / 16)
      if (gy + 1 < GH && gx + 1 < GW) spread(idx + GW + 1, er, eg, eb, 1 / 16)
      final[idx] = p
    }
  } else if (opts.dither) {
    // Ordered (bayer / bayer8 / diag): structured, flat-areas-stay-flat.
    final = orderedDither(cellRGB, palette, GW, GH, opts.dither)
  } else {
    final = new Array(GW * GH)
    for (let idx = 0; idx < GW * GH; idx++) {
      const c = cellRGB[idx]
      final[idx] = c ? palette[nearest(palette, c[0], c[1], c[2])] : null
    }
  }

  // Optional speckle cleanup (after dithering would erase the dither, so skip
  // there unless explicitly asked — dither intentionally makes lots of islands).
  let cleaned = 0
  if (opts.clean) cleaned = cleanIslands(final, GW, GH, opts.clean)

  // Pack to an RGBA buffer + count colors actually used.
  const out = Buffer.alloc(GW * GH * 4)
  const used = new Set()
  for (let idx = 0; idx < GW * GH; idx++) {
    const o = idx * 4, c = final[idx]
    if (!c) { out[o] = out[o + 1] = out[o + 2] = out[o + 3] = 0; continue }
    out[o] = c[0]; out[o + 1] = c[1]; out[o + 2] = c[2]; out[o + 3] = 255
    used.add(`${c[0]},${c[1]},${c[2]}`)
  }

  // Encode (nearest-neighbor upscale via sharp kernel).
  let pipe = sharp(out, { raw: { width: GW, height: GH, channels: 4 } })
  if (opts.scale > 1) {
    pipe = pipe.resize(GW * opts.scale, GH * opts.scale, { kernel: 'nearest' })
  }
  await pipe.png().toFile(output)

  console.error(`pixelsnap: ${opts.input}  ${W}×${H}`)
  console.error(`  grid     ${GW}×${GH} logical pixels` +
    (opts.grid ? ' (--grid)' : opts.cell ? ` (--cell ${opts.cell})` : ` (auto, cell ≈ ${(W / GW).toFixed(1)}px; ${autoNote})`))
  console.error(`  palette  ${paletteSrc}, ${used.size} colors used`)
  console.error(`  sample   ${opts.sample}${opts.dither ? ` + ${opts.dither} dither` : ''}${opts.clean ? ` + clean ≤${opts.clean} (${cleaned} cells)` : ''}`)
  console.error(`  wrote    ${output}  ${GW * opts.scale}×${GH * opts.scale}`)
}

main().catch(e => die(e.stack || e.message))
