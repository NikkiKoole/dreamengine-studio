#!/usr/bin/env node
// voxel-bake.js — bake tiny ASCII voxel models into ROTATED sprite cells + an atlas.
//
// WHY THIS EXISTS. An isometric room that ROTATES costs one drawing per object per
// rotation. Hand-drawn that is 8 sprites for every sofa, bed and fridge, which is where
// such a project dies. And the obvious fix — render the voxels with tritex every frame —
// is the one thing measured too slow on device (~89ms/frame on an iPhone SE, ADR-0024).
// So: author each object ONCE as ASCII voxel layers, and bake all its rotations at BUILD
// time into flat sprite cells. Runtime becomes sspr() + a painter's sort and never
// touches a triangle. This is 1999's pipeline (RollerCoaster Tycoon and The Sims 1 both
// pre-rendered 3D models to sprite sets), which is the right era to borrow from here.
// Full rationale, the two projection families and the go/no-go: docs/design/iso-rooms.md
//
// THE EIGHT ROTATIONS ARE TWO FAMILIES, not one thing:
//   DIAGONAL (the four 45° steps) — tile is a 2:1 diamond, two box faces + top visible.
//   CARDINAL (the four 90° steps) — tile is an axis-aligned rect, one face + top.
// Both land on integer pixels, which is why the set is 8: everything between them
// (22.5° and friends) aliases. Rotation index r = 0..7 alternates CARDINAL (even) and
// DIAGONAL (odd), so r*45° reads as a real turn.
//
// LIGHT IS FIXED IN SCREEN SPACE, ON PURPOSE. A face is shaded by which way it points
// ON SCREEN (top / screen-left / screen-right), not by its world normal. Shade by world
// normal and the light appears to rotate with the room as you turn it, which is the
// classic iso bug. Doing it in screen space means the "light must not rotate" oracle
// passes by construction rather than by luck.
//
// MODEL FORMAT — a .js module, or a { materials, models } object:
//   module.exports = {
//     materials: {                        // char -> colour
//       w: 6,                             //   a pico32 index: side tones auto-derived
//       c: [10, 9, 4],                    //   or explicit [top, screenRight, screenLeft]
//     },
//     models: {
//       stool: { layers: [                // layers[0] is the BOTTOM (z=0), going up
//         ['ww',                          //   a layer is rows of chars; row = +y, char = +x
//          'ww'],
//         3,                              //   a NUMBER repeats the previous layer N times
//         ['cc',
//          'cc'],
//       ]},
//     },
//   }
// '.' and ' ' are empty. Every other char must appear in `materials`. The repeat shorthand
// matters more than it looks: a 16-voxel-tall fridge is 16 identical layers, and spelling
// those out is 128 lines of ASCII nobody will proofread.
//
// USAGE
//   node tools/voxel-bake.js <models.js> [options]
//   node tools/voxel-bake.js --check              # known-answer self-test, bakes nothing
//
// OPTIONS
//   --out <base>     write <base>.png (atlas) + <base>.json (index). Default: no write.
//   --emit-c <path>  write the cart-side C table (IsoCell rects + origins + footprints).
//                    Carries a SIGNATURE the cart's .cart.js re-derives, so a stale header
//                    is a loud error instead of furniture drawn with last week's rectangles.
//   --tw <px>        DIAGONAL tile width in px (default 24; height is tw/2, the 2:1 form)
//   --cw <px>        CARDINAL tile width in px (default 17 ≈ tw/√2, see --true-scale)
//   --zh <px>        pixels per voxel of HEIGHT (default 12)
//   --true-scale     set cw = tw/√2 exactly, the geometrically honest ratio, which makes
//                    the room appear to ZOOM as you rotate between families. Off by
//                    default: cw defaults to a rounded value and the pop is absorbed.
//   --rots <list>    comma list of rotation indices to bake (default 0-7)
//   --scale <n>      upscale the written atlas n× nearest-neighbour, for eyeballing
//   --report         per-model cell dims + TOTAL ATLAS PIXELS (the real budget — see
//                    the note on sheet size below) and exit
//   --json           machine-readable report on stdout
//
// SHEET SIZE, THE THING THAT WILL BITE — LESS THAN IT DID. make-cart.js DEFAULTS a cart's
// sheet to 128×128 (8×8 grid of 16×16 slots) and drops any slot index >= 64, BUT since
// 2026-08-12 a `.cart.js` may declare a bigger one (`sheet: {w,h}`, or `sheet: 256` for
// square) and the cap scales with it. So a generator cart CAN ship a wider atlas; the
// sprite editor still assumes the 8×8 grid, so this is a generator-only escape. The runtime
// was never the limit (`cols` is derived from the loaded sheet's real width, and sspr()
// addresses any sub-rect). This tool reports total atlas pixels so the decision to widen
// that cap is made against a measurement instead of an estimate. 128×128 = 16384 px.
'use strict'

const fs   = require('fs')
const path = require('path')

// ── palette ───────────────────────────────────────────────────
const PAL_PATH = path.join(__dirname, '..', 'editor', 'public', 'palettes', 'pico32.json')
let PALETTE = null
function palette() {
  if (!PALETTE) {
    const j = JSON.parse(fs.readFileSync(PAL_PATH, 'utf8'))
    PALETTE = (j.palette || j).map(h => {
      const s = h.replace('#', '')
      return [parseInt(s.slice(0, 2), 16), parseInt(s.slice(2, 4), 16), parseInt(s.slice(4, 6), 16)]
    })
  }
  return PALETTE
}

const LUM = c => 0.299 * c[0] + 0.587 * c[1] + 0.114 * c[2]
const CHROMA = c => Math.max(c[0], c[1], c[2]) - Math.min(c[0], c[1], c[2])
// Hue as a direction in the r:g:b simplex. Cheap, and unlike an HSV angle it degrades
// gracefully toward neutral instead of going undefined.
const DIR = c => { const s = c[0] + c[1] + c[2] || 1; return [c[0] / s, c[1] / s, c[2] / s] }

// One step DOWN a tone ramp: the palette entry closest to `factor` × the base luminance
// that keeps the base's hue and is strictly darker than `ceilIdx`.
//
// Naive nearest-RGB gets this wrong, and the failure is ugly rather than subtle: darkening
// a neutral light grey (#c2c3c7) lands on pico32's indigo (#83769c), because indigo really
// is closest in raw distance. Every grey wall and white fridge baked out LAVENDER. Three
// things fix it, and all three are needed:
//   1. match on LUMINANCE, since that is what a ramp step actually is;
//   2. penalise hue and chroma drift hard (6x), so a neutral stays neutral;
//   3. require the step to be strictly DARKER, or the search degenerates to returning the
//      base colour again and the cube comes out flat.
// pico32 does contain a usable neutral ramp (7 -> 6 -> 5 -> 21 -> 16 -> 0); the matcher
// just has to be told to look for it.
function rampStep(baseIdx, factor, ceilIdx) {
  const P = palette()
  const base = P[baseIdx], ceil = P[ceilIdx]
  const targetLum = LUM(base) * factor
  const bd = DIR(base), bc = CHROMA(base)
  let best = -1, bestScore = Infinity
  for (let i = 0; i < P.length; i++) {
    const p = P[i]
    if (LUM(p) >= LUM(ceil) - 8) continue             // must be a real step down
    const pd = DIR(p)
    const hueErr = (pd[0] - bd[0]) ** 2 + (pd[1] - bd[1]) ** 2 + (pd[2] - bd[2]) ** 2
    const score = (LUM(p) - targetLum) ** 2
                + 6 * (hueErr * 255 * 255 + (CHROMA(p) - bc) ** 2)
    if (score < bestScore) { bestScore = score; best = i }
  }
  return best < 0 ? baseIdx : best                    // a black base has nowhere to go
}

// One material char -> [topIdx, screenRightIdx, screenLeftIdx].
// A bare index auto-derives its two side tones, so a model can name one colour and still
// get a lit cube. Explicit triples override it, which is what to reach for when a specific
// object needs a specific reading.
function materialTones(spec) {
  if (Array.isArray(spec)) {
    if (spec.length !== 3) throw new Error(`material array must be [top,right,left], got ${spec.length}`)
    return spec.slice()
  }
  if (!Number.isInteger(spec) || spec < 0 || spec > 31) throw new Error(`material must be a pico index 0..31 or [top,right,left], got ${JSON.stringify(spec)}`)
  const mid = rampStep(spec, 0.72, spec)
  const dark = rampStep(spec, 0.46, mid)
  return [spec, mid, dark]
}

// ── model parsing ─────────────────────────────────────────────
// layers[0] = bottom. Within a layer, index = +y (down/back), char = +x (right).
// Returns { nx, ny, nz, at(x,y,z) -> char|null }.
function parseModel(name, def, materials) {
  if (!def || !Array.isArray(def.layers) || def.layers.length === 0) throw new Error(`model "${name}": needs a non-empty layers[]`)
  // Expand the repeat shorthand: a NUMBER entry means "repeat the previous layer N more
  // times". A 16-voxel-tall fridge is 16 identical layers, and spelling those out is 128
  // lines of ASCII nobody will proofread.
  const layers = []
  def.layers.forEach((layer, i) => {
    if (typeof layer === 'number') {
      if (layers.length === 0) throw new Error(`model "${name}": entry ${i} is a repeat count with no layer before it`)
      if (!Number.isInteger(layer) || layer < 1) throw new Error(`model "${name}": repeat count at entry ${i} must be a positive integer`)
      for (let k = 0; k < layer; k++) layers.push(layers[layers.length - 1])
    } else layers.push(layer)
  })
  const nz = layers.length
  let nx = 0, ny = 0
  layers.forEach((layer, z) => {
    if (!Array.isArray(layer)) throw new Error(`model "${name}": layer ${z} must be an array of row strings`)
    ny = Math.max(ny, layer.length)
    layer.forEach(row => {
      if (typeof row !== 'string') throw new Error(`model "${name}": layer ${z} rows must be strings`)
      nx = Math.max(nx, row.length)
    })
  })
  const solid = new Map()
  layers.forEach((layer, z) => {
    layer.forEach((row, y) => {
      for (let x = 0; x < row.length; x++) {
        const ch = row[x]
        if (ch === '.' || ch === ' ') continue
        if (!(ch in materials)) throw new Error(`model "${name}": char '${ch}' at layer ${z} row ${y} is not in materials`)
        solid.set(`${x},${y},${z}`, ch)
      }
    })
  })
  if (solid.size === 0) throw new Error(`model "${name}": no solid voxels`)
  return { name, nx, ny, nz, count: solid.size, at: (x, y, z) => solid.get(`${x},${y},${z}`) || null }
}

// ── projection ────────────────────────────────────────────────
// Eight rotations as EXPLICIT integer-friendly linear maps, rather than trig, so the
// diamonds stay pixel-crisp. Even r = CARDINAL, odd r = DIAGONAL; r increases by 45°.
//
// A world point (x,y,z) in voxel units projects to screen (sx, sy) as
//   sx = ax*x + ay*y
//   sy = bx*x + by*y - zh*z
// The world (x,y) is first turned by the rotation's quarter-turn, which is folded into
// the coefficients below. Depth (what is in front) uses the same turned axes.
function projector(r, opt) {
  const tw = opt.tw, th = opt.tw / 2, cw = opt.cw, zh = opt.zh
  const q = r >> 1                                  // which quarter turn, 0..3
  const diag = (r & 1) === 1
  // Quarter-turn of the world (x,y) plane: (x,y) -> (X,Y)
  const turn = (x, y) => {
    switch (q) {
      case 0: return [x, y]
      case 1: return [-y, x]
      case 2: return [-x, -y]
      default: return [y, -x]
    }
  }
  const project = diag
    ? (x, y, z) => { const [X, Y] = turn(x, y); return [(X - Y) * (tw / 2), (X + Y) * (th / 2) - z * zh] }
    : (x, y, z) => { const [X, Y] = turn(x, y); return [X * cw, Y * (cw / 2) - z * zh] }
  // Depth: larger = nearer the camera. In both families the camera looks down the +X+Y
  // diagonal (diag) or down +Y (cardinal) of the TURNED axes.
  const depth = diag
    ? (x, y, z) => { const [X, Y] = turn(x, y); return X + Y + z * 0.001 }
    : (x, y, z) => { const [X, Y] = turn(x, y); return Y + z * 0.001 }
  return { project, depth, diag }
}

// The six cube faces, as [outward normal, the 4 corner offsets in CCW order].
const FACES = [
  { n: [0, 0, 1], v: [[0, 0, 1], [1, 0, 1], [1, 1, 1], [0, 1, 1]] },   // top
  { n: [0, 0, -1], v: [[0, 0, 0], [0, 1, 0], [1, 1, 0], [1, 0, 0]] },  // bottom
  { n: [1, 0, 0], v: [[1, 0, 0], [1, 1, 0], [1, 1, 1], [1, 0, 1]] },   // +x
  { n: [-1, 0, 0], v: [[0, 0, 0], [0, 0, 1], [0, 1, 1], [0, 1, 0]] },  // -x
  { n: [0, 1, 0], v: [[0, 1, 0], [0, 1, 1], [1, 1, 1], [1, 1, 0]] },   // +y
  { n: [0, -1, 0], v: [[0, 0, 0], [1, 0, 0], [1, 0, 1], [0, 0, 1]] },  // -y
]

// Is this face pointing at the camera, and which SCREEN-SPACE tone does it get?
// Tone 0 = top, 1 = screen-right (or the single front face in a cardinal view),
// 2 = screen-left. Screen space is the whole point: it keeps the light from turning with
// the room, which is the classic iso bug.
//
// VISIBILITY uses the projector's own depth function rather than a polygon area test:
// a face points at the camera exactly when stepping along its outward normal gets you
// NEARER. That also culls the edge-on faces for free — in a cardinal view the ±x faces
// project to a zero-width line, and depth (which ignores x there) reports no change. An
// area test can be fooled into calling those "screen-right facing", which is the bug this
// replaced.
function faceTone(face, proj) {
  if (face.n[2] === -1) return null                   // bottom is never visible
  const cx = 0.5 + face.n[0] * 0.5, cy = 0.5 + face.n[1] * 0.5, cz = 0.5 + face.n[2] * 0.5
  const here = proj.depth(cx, cy, cz)
  const ahead = proj.depth(cx + face.n[0], cy + face.n[1], cz + face.n[2])
  if (ahead - here <= 1e-9) return null               // edge-on, or facing away
  if (face.n[2] === 1) return 0                       // top reads as lit
  const [ax] = proj.project(face.n[0], face.n[1], 0)
  const [ox] = proj.project(0, 0, 0)
  const dx = ax - ox
  return dx >= 0 ? 1 : 2
}

// ── rasterizing one cell ──────────────────────────────────────
// Fill a convex polygon of screen points into a palette-index buffer. Half-open scanline
// on pixel centres, so adjacent faces tile without seams or double-writes.
function fillPoly(buf, W, H, pts, idx) {
  let minY = Infinity, maxY = -Infinity
  for (const p of pts) { if (p[1] < minY) minY = p[1]; if (p[1] > maxY) maxY = p[1] }
  const y0 = Math.max(0, Math.ceil(minY - 0.5)), y1 = Math.min(H - 1, Math.floor(maxY - 0.5))
  for (let y = y0; y <= y1; y++) {
    const cy = y + 0.5
    let lo = Infinity, hi = -Infinity
    for (let i = 0; i < pts.length; i++) {
      const a = pts[i], b = pts[(i + 1) % pts.length]
      if ((a[1] <= cy && b[1] > cy) || (b[1] <= cy && a[1] > cy)) {
        const t = (cy - a[1]) / (b[1] - a[1])
        const x = a[0] + t * (b[0] - a[0])
        if (x < lo) lo = x
        if (x > hi) hi = x
      }
    }
    if (lo > hi) continue
    const x0 = Math.max(0, Math.ceil(lo - 0.5)), x1 = Math.min(W - 1, Math.floor(hi - 0.5))
    for (let x = x0; x <= x1; x++) buf[y * W + x] = idx
  }
}

// Bake ONE model at ONE rotation. Returns a tight-cropped cell:
//   { w, h, ox, oy, px } where px is a w*h array of palette indices (-1 transparent)
// and (ox,oy) is where the model's FLOOR-ORIGIN (voxel 0,0,0's projected corner) sits
// inside the cell, so the cart can place a cell without re-deriving the projection.
function bakeCell(model, r, materials, opt) {
  const proj = projector(r, opt)
  // Pass 1: screen bounds over every corner of every voxel.
  let minX = Infinity, minY = Infinity, maxX = -Infinity, maxY = -Infinity
  for (let z = 0; z < model.nz; z++) for (let y = 0; y < model.ny; y++) for (let x = 0; x < model.nx; x++) {
    if (!model.at(x, y, z)) continue
    for (const dz of [0, 1]) for (const dy of [0, 1]) for (const dx of [0, 1]) {
      const [sx, sy] = proj.project(x + dx, y + dy, z + dz)
      if (sx < minX) minX = sx; if (sx > maxX) maxX = sx
      if (sy < minY) minY = sy; if (sy > maxY) maxY = sy
    }
  }
  const W = Math.max(1, Math.round(maxX - minX)), H = Math.max(1, Math.round(maxY - minY))
  const px = new Int8Array(W * H).fill(-1)
  // Pass 2: painter's order, farthest first. Exact for a voxel grid under an orthographic
  // axonometric projection — there are no depth cycles to worry about.
  const cells = []
  for (let z = 0; z < model.nz; z++) for (let y = 0; y < model.ny; y++) for (let x = 0; x < model.nx; x++) {
    const ch = model.at(x, y, z)
    if (ch) cells.push({ x, y, z, ch, d: proj.depth(x, y, z) })
  }
  cells.sort((a, b) => a.d - b.d)
  const tones = {}
  for (const ch of Object.keys(materials)) tones[ch] = materialTones(materials[ch])
  for (const c of cells) {
    for (const face of FACES) {
      const tone = faceTone(face, proj)
      if (tone === null) continue
      // Skip a face buried against a neighbour: saves nothing at bake time, but it keeps
      // interior faces from painting over an already-drawn outer face at shared edges.
      const nb = model.at(c.x + face.n[0], c.y + face.n[1], c.z + face.n[2])
      if (nb) continue
      const pts = face.v.map(([dx, dy, dz]) => {
        const [sx, sy] = proj.project(c.x + dx, c.y + dy, c.z + dz)
        return [sx - minX, sy - minY]
      })
      fillPoly(px, W, H, pts, tones[c.ch][tone])
    }
  }
  const [zx, zy] = proj.project(0, 0, 0)
  return { w: W, h: H, ox: Math.round(zx - minX), oy: Math.round(zy - minY), px }
}

// ── atlas packing ─────────────────────────────────────────────
// Shelf / next-fit-decreasing-height. Not optimal, but it is stable (same input -> same
// atlas) which matters more here: a shifting atlas would churn every baked cart.
function packAtlas(cells, maxW) {
  const order = cells.map((c, i) => ({ i, w: c.w, h: c.h })).sort((a, b) => b.h - a.h || b.w - a.w || a.i - b.i)
  const place = new Array(cells.length)
  let shelfY = 0, shelfH = 0, penX = 0, usedW = 0
  for (const c of order) {
    if (penX + c.w > maxW) { shelfY += shelfH; shelfH = 0; penX = 0 }
    place[c.i] = { x: penX, y: shelfY }
    penX += c.w
    if (c.h > shelfH) shelfH = c.h
    if (penX > usedW) usedW = penX
  }
  return { place, w: usedW, h: shelfY + shelfH }
}

// ── the bake ──────────────────────────────────────────────────
function bake(spec, opt) {
  const materials = spec.materials || {}
  const rots = opt.rots
  const out = { tw: opt.tw, th: opt.tw / 2, cw: opt.cw, zh: opt.zh, rots, models: {} }
  const cells = []
  for (const [name, def] of Object.entries(spec.models || {})) {
    const model = parseModel(name, def, materials)
    out.models[name] = { nx: model.nx, ny: model.ny, nz: model.nz, voxels: model.count, rots: {} }
    for (const r of rots) {
      const cell = bakeCell(model, r, materials, opt)
      out.models[name].rots[r] = { w: cell.w, h: cell.h, ox: cell.ox, oy: cell.oy }
      cells.push({ name, r, ...cell })
    }
  }
  if (cells.length === 0) throw new Error('nothing to bake: models{} is empty')
  const { place, w, h } = packAtlas(cells, opt.atlasW)
  cells.forEach((c, i) => {
    const p = place[i]
    out.models[c.name].rots[c.r].x = p.x
    out.models[c.name].rots[c.r].y = p.y
  })
  out.atlas = { w, h, pixels: w * h, cellPixels: cells.reduce((s, c) => s + c.w * c.h, 0) }
  return { index: out, cells, place, atlasW: w, atlasH: h }
}

// A signature over everything that changes the bake. The generated C header carries it and
// the cart's .cart.js re-derives it, so a stale header is a loud error instead of a cart
// drawing furniture with last week's rectangles.
function signature(spec, opt) {
  const models = Object.keys(spec.models || {}).sort()
  const shape = models.map(m => `${m}:${JSON.stringify(spec.models[m].layers)}`).join('|')
  const mats = JSON.stringify(Object.keys(spec.materials || {}).sort().map(k => [k, spec.materials[k]]))
  let h = 2166136261
  for (const ch of `${opt.tw},${opt.cw},${opt.zh},${opt.rots.join('')},${opt.atlasW}|${mats}|${shape}`) {
    h ^= ch.charCodeAt(0); h = Math.imul(h, 16777619)
  }
  return (h >>> 0).toString(16).padStart(8, '0')
}

// Emit the cart-side lookup table. The runtime needs, per model per rotation, the sub-rect
// to sspr() out of the sheet plus where the model's floor origin sits inside that rect.
function emitC(res, spec, opt, outPath) {
  const idx = res.index
  const names = Object.keys(idx.models)
  const L = []
  L.push('// GENERATED by tools/voxel-bake.js — do not hand-edit.')
  L.push(`// Regenerate: node tools/voxel-bake.js <models.js> --tw ${opt.tw} --zh ${opt.zh} --emit-c ${outPath}`)
  L.push('// Baked rotation cells for an isometric room. See docs/design/iso-rooms.md.')
  L.push('#pragma once')
  L.push('')
  L.push(`#define ISO_SIG      "${signature(spec, opt)}"   // guard: the .cart.js re-derives this`)
  L.push(`#define ISO_ATLAS_W  ${idx.atlas.w}`)
  L.push(`#define ISO_ATLAS_H  ${idx.atlas.h}`)
  L.push(`#define ISO_TW       ${opt.tw}    // diagonal tile width, px per voxel unit`)
  L.push(`#define ISO_TH       ${opt.tw / 2}`)
  L.push(`#define ISO_CW       ${opt.cw}    // cardinal tile width (== ISO_TW keeps the footprint equal)`)
  L.push(`#define ISO_ZH       ${opt.zh}    // px per voxel of height`)
  L.push(`#define ISO_ROTS     ${opt.rots.length}`)
  L.push('')
  L.push('// One baked cell: where it is in the sheet, and where the model\'s floor origin')
  L.push('// (voxel 0,0,0\'s projected corner) sits inside it, so placing a cell is a subtraction.')
  L.push('typedef struct { short x, y, w, h, ox, oy; } IsoCell;')
  L.push('')
  L.push('typedef enum {')
  names.forEach((n, i) => L.push(`    ISO_${n.toUpperCase()} = ${i},`))
  L.push(`    ISO_MODEL_COUNT = ${names.length}`)
  L.push('} IsoModel;')
  L.push('')
  L.push(`static const char *ISO_NAMES[ISO_MODEL_COUNT] = { ${names.map(n => `"${n}"`).join(', ')} };`)
  L.push('')
  L.push('// [model][rotation]. Rotation is 0..7; EVEN = cardinal, ODD = diagonal.')
  L.push('static const IsoCell ISO_CELLS[ISO_MODEL_COUNT][ISO_ROTS] = {')
  for (const n of names) {
    const rows = opt.rots.map(r => {
      const c = idx.models[n].rots[r]
      return `{${c.x},${c.y},${c.w},${c.h},${c.ox},${c.oy}}`
    })
    L.push(`    /* ${n.padEnd(10)} */ { ${rows.join(', ')} },`)
  }
  L.push('};')
  L.push('')
  L.push('// Voxel footprint per model, in voxel units (8 voxels = one floor tile).')
  L.push('static const short ISO_FOOTPRINT[ISO_MODEL_COUNT][3] = {')
  for (const n of names) {
    const m = idx.models[n]
    L.push(`    /* ${n.padEnd(10)} */ {${m.nx},${m.ny},${m.nz}},`)
  }
  L.push('};')
  L.push('')
  fs.writeFileSync(outPath, L.join('\n'))
  return outPath
}

// Composite the packed cells into one flat atlas of PALETTE INDICES (-1 = transparent).
// This is what a cart's .cart.js hands to make-cart as `atlas: { w, h, px }`; writeAtlas
// below turns the same buffer into a PNG. One compositing path, so the eyeball preview and
// the shipped sheet cannot disagree.
function atlasPixels(res) {
  const W = res.atlasW, H = res.atlasH
  const px = new Array(W * H).fill(-1)
  res.cells.forEach((c, i) => {
    const p = res.place[i]
    for (let y = 0; y < c.h; y++) for (let x = 0; x < c.w; x++) {
      const v = c.px[y * c.w + x]
      if (v >= 0) px[(p.y + y) * W + (p.x + x)] = v
    }
  })
  return { w: W, h: H, px }
}

async function writeAtlas(res, base, scale) {
  const sharp = require('sharp')
  const P = palette()
  const { w: W, h: H, px } = atlasPixels(res)
  const rgba = Buffer.alloc(W * H * 4, 0)
  for (let i = 0; i < W * H; i++) {
    const v = px[i]
    if (v < 0) continue
    const o = i * 4
    rgba[o] = P[v][0]; rgba[o + 1] = P[v][1]; rgba[o + 2] = P[v][2]; rgba[o + 3] = 255
  }
  let img = sharp(rgba, { raw: { width: W, height: H, channels: 4 } })
  if (scale > 1) img = img.resize(W * scale, H * scale, { kernel: 'nearest' })
  await img.png().toFile(base + '.png')
  fs.writeFileSync(base + '.json', JSON.stringify(res.index, null, 2) + '\n')
  return { png: base + '.png', json: base + '.json' }
}

// ── self-test ─────────────────────────────────────────────────
// Known answers over synthetic models, so a broken projection is caught without needing
// a cart, an eye, or a baked frame. Runs no models file.
function selfcheck() {
  let pass = 0, fail = 0
  const ok = (name, cond, extra = '') => { if (cond) { pass++ } else { fail++; console.log(`  FAIL ${name} ${extra}`) } }
  const opt = { tw: 24, cw: 17, zh: 12, rots: [0, 1, 2, 3, 4, 5, 6, 7], atlasW: 256 }

  // --- geometry: a single voxel ---
  const one = { materials: { a: 6 }, models: { one: { layers: [['a']] } } }
  const m = parseModel('one', one.models.one, one.materials)
  ok('parse: 1x1x1', m.nx === 1 && m.ny === 1 && m.nz === 1 && m.count === 1, `${m.nx}x${m.ny}x${m.nz}`)

  // A diagonal view of one voxel spans the full tile width, and its top face is 2:1.
  const d1 = bakeCell(m, 1, one.materials, opt)
  ok('diag cell width == tw', d1.w === 24, `got ${d1.w}`)
  ok('diag cell height == th + zh', d1.h === 12 + 12, `got ${d1.h}`)
  // A cardinal view of one voxel is cw wide.
  const c0 = bakeCell(m, 0, one.materials, opt)
  ok('cardinal cell width == cw', c0.w === 17, `got ${c0.w}`)

  // --- the 2:1 diamond is exact, which is why 45° is a legal angle at all ---
  const projD = projector(1, opt)
  const [px1] = projD.project(1, 0, 0), [px0] = projD.project(0, 0, 0)
  const [, py1] = projD.project(1, 0, 0), [, py0] = projD.project(0, 0, 0)
  ok('diag step is 2:1', Math.abs((px1 - px0) / (py1 - py0)) === 2, `${px1 - px0}:${py1 - py0}`)

  // --- LIGHT MUST NOT ROTATE. The tone of the screen-left face has to be the same at
  // every rotation; if shading were done by world normal it would cycle instead. ---
  const toneSets = []
  for (let r = 0; r < 8; r++) {
    const proj = projector(r, opt)
    const seen = new Set()
    for (const f of FACES) { const t = faceTone(f, proj); if (t !== null) seen.add(t) }
    toneSets.push([...seen].sort().join(''))
  }
  ok('every rotation shows a top face', toneSets.every(s => s.includes('0')), toneSets.join(' '))
  ok('diagonals show both side tones', [1, 3, 5, 7].every(r => toneSets[r] === '012'), toneSets.join(' '))
  ok('cardinals show exactly one side tone', [0, 2, 4, 6].every(r => toneSets[r].length === 2), toneSets.join(' '))

  // The real invariant: for a single voxel, the count of pixels at each tone must be
  // IDENTICAL across all four diagonal rotations. Same shape, same light, turned.
  const hist = r => {
    const c = bakeCell(m, r, one.materials, opt)
    const h = {}
    for (const v of c.px) if (v >= 0) h[v] = (h[v] || 0) + 1
    return JSON.stringify(Object.entries(h).sort())
  }
  const hd = [1, 3, 5, 7].map(hist)
  ok('light fixed across diagonal rotations', new Set(hd).size === 1, hd.join(' | '))
  const hc = [0, 2, 4, 6].map(hist)
  ok('light fixed across cardinal rotations', new Set(hc).size === 1, hc.join(' | '))

  // --- materials ---
  const t = materialTones(6)                          // light grey
  ok('auto ramp is 3 tones', t.length === 3)
  ok('auto ramp darkens', palette()[t[1]][0] < palette()[t[0]][0] && palette()[t[2]][0] < palette()[t[1]][0], JSON.stringify(t))
  ok('explicit tones pass through', JSON.stringify(materialTones([1, 2, 3])) === '[1,2,3]')
  // The ramp must not hue-shift. Darkening a NEUTRAL grey has to stay neutral: nearest-RGB
  // alone picks pico32's indigo here and bakes lavender walls, which is the bug this pins.
  const greyRamp = materialTones(6)                   // #c2c3c7, as neutral as pico32 gets
  const chroma = c => Math.max(...c) - Math.min(...c)
  ok('grey ramp stays neutral', greyRamp.every(i => chroma(palette()[i]) <= 24),
     greyRamp.map(i => `${i}:${chroma(palette()[i])}`).join(' '))
  // And a saturated colour must keep its hue family: brown stays warm (red >= blue).
  const brownRamp = materialTones(4)                  // #ab5236
  ok('brown ramp stays warm', brownRamp.every(i => palette()[i][0] >= palette()[i][2]),
     brownRamp.map(i => `${i}:${palette()[i]}`).join(' '))
  let threw = false
  try { materialTones([1, 2]) } catch (e) { threw = true }
  ok('rejects a 2-tone material', threw)
  threw = false
  try { materialTones(99) } catch (e) { threw = true }
  ok('rejects an out-of-range index', threw)
  threw = false
  try { parseModel('bad', { layers: [['x']] }, { a: 6 }) } catch (e) { threw = true }
  ok('rejects an unknown material char', threw)
  threw = false
  try { parseModel('empty', { layers: [['..']] }, { a: 6 }) } catch (e) { threw = true }
  ok('rejects an all-empty model', threw)

  // --- the repeat shorthand ---
  const spelled = parseModel('spelled', { layers: [['aa'], ['aa'], ['aa']] }, { a: 6 })
  const repeated = parseModel('repeated', { layers: [['aa'], 2] }, { a: 6 })
  ok('repeat shorthand == spelled out', repeated.nz === spelled.nz && repeated.count === spelled.count,
     `${repeated.nz}/${repeated.count} vs ${spelled.nz}/${spelled.count}`)
  threw = false
  try { parseModel('bad', { layers: [3, ['aa']] }, { a: 6 }) } catch (e) { threw = true }
  ok('repeat with nothing before it throws', threw)
  threw = false
  try { parseModel('bad', { layers: [['aa'], 0] }, { a: 6 }) } catch (e) { threw = true }
  ok('repeat count must be positive', threw)

  // --- NO ZOOM POP between the two families when cw == tw. A tile has to cover the same
  // screen footprint in a diagonal and a cardinal view, or rotating looks like zooming. ---
  const flat = parseModel('flat', { layers: [['aaaaaaaa', 'aaaaaaaa', 'aaaaaaaa', 'aaaaaaaa',
                                              'aaaaaaaa', 'aaaaaaaa', 'aaaaaaaa', 'aaaaaaaa']] }, { a: 6 })
  const noPop = { tw: 24, cw: 24, zh: 12, rots: [0, 1], atlasW: 256 }
  const fd = bakeCell(flat, 1, { a: 6 }, noPop), fc = bakeCell(flat, 0, { a: 6 }, noPop)
  ok('cw==tw keeps the tile footprint width equal across families', fd.w === fc.w, `diag ${fd.w} vs cardinal ${fc.w}`)
  const popped = { tw: 24, cw: 24 / Math.SQRT2, zh: 12, rots: [0, 1], atlasW: 256 }
  const pc = bakeCell(flat, 0, { a: 6 }, popped)
  ok('--true-scale really does shrink the cardinal view', pc.w < fd.w, `${pc.w} vs ${fd.w}`)

  // --- a taller model is taller on screen, by exactly zh per voxel ---
  const tall = parseModel('tall', { layers: [['a'], ['a'], ['a']] }, { a: 6 })
  const t3 = bakeCell(tall, 1, { a: 6 }, opt)
  ok('height grows by zh per layer', t3.h === d1.h + 2 * opt.zh, `${t3.h} vs ${d1.h}+24`)

  // --- occlusion: a 2x2x1 slab has FEWER pixels than 4 separate voxels would, because
  // shared faces are skipped. Catches a baker that draws interior faces. ---
  const slab = parseModel('slab', { layers: [['aa', 'aa']] }, { a: 6 })
  const s = bakeCell(slab, 1, { a: 6 }, opt)
  let solidS = 0; for (const v of s.px) if (v >= 0) solidS++
  let solid1 = 0; for (const v of d1.px) if (v >= 0) solid1++
  ok('slab is not 4x a single voxel (shared faces skipped)', solidS < solid1 * 4, `${solidS} vs ${solid1 * 4}`)
  ok('slab is bigger than one voxel', solidS > solid1, `${solidS} vs ${solid1}`)

  // --- atlas packing is deterministic and lossless ---
  const twoBake = bake({ materials: { a: 6 }, models: { one: { layers: [['a']] }, tall: { layers: [['a'], ['a']] } } }, opt)
  const again = bake({ materials: { a: 6 }, models: { one: { layers: [['a']] }, tall: { layers: [['a'], ['a']] } } }, opt)
  ok('pack is deterministic', JSON.stringify(twoBake.index) === JSON.stringify(again.index))
  ok('atlas fits its cells', twoBake.index.atlas.pixels >= twoBake.index.atlas.cellPixels,
     `${twoBake.index.atlas.pixels} < ${twoBake.index.atlas.cellPixels}`)
  const rects = twoBake.cells.map((c, i) => ({ ...twoBake.place[i], w: c.w, h: c.h }))
  let overlap = false
  for (let i = 0; i < rects.length; i++) for (let j = i + 1; j < rects.length; j++) {
    const a = rects[i], b = rects[j]
    if (a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h) overlap = true
  }
  ok('packed cells do not overlap', !overlap)
  ok('every model x rotation got a cell', Object.values(twoBake.index.models).every(md => Object.keys(md.rots).length === 8))

  // --- the origin marker ---
  // For a model whose (0,0,0) voxel is SOLID the origin lands inside the tight crop...
  ok('origin inside cell when (0,0,0) is solid',
     d1.ox >= 0 && d1.ox <= d1.w && d1.oy >= 0 && d1.oy <= d1.h, `${d1.ox},${d1.oy} in ${d1.w}x${d1.h}`)
  // ...but when (0,0,0) is EMPTY it legitimately falls outside, because the origin is a
  // reference corner rather than a pixel and the crop only covers the silhouette. Placement
  // (dx = sx - ox) is sign-agnostic, so this is correct, not a bug — pinned here because the
  // over-strict version of this assertion cost a false failure in isoroom's spec.
  const offset = parseModel('offset', { layers: [['.a', '..']] }, { a: 6 })
  const od = bakeCell(offset, 1, { a: 6 }, opt)
  ok('origin may fall outside the crop when (0,0,0) is empty',
     od.ox !== 0 || od.oy !== 0, `${od.ox},${od.oy} in ${od.w}x${od.h}`)
  ok('...but stays within a tile of it',
     od.ox >= -opt.tw * 2 && od.ox <= od.w + opt.tw * 2 &&
     od.oy >= -opt.tw * 2 && od.oy <= od.h + opt.tw * 2, `${od.ox},${od.oy} in ${od.w}x${od.h}`)

  console.log(`\nvoxel-bake --check: ${pass} passed, ${fail} failed`)
  return fail === 0
}

// ── cli ───────────────────────────────────────────────────────
function parseArgs(argv) {
  const o = { tw: 24, cw: null, zh: 12, rots: [0, 1, 2, 3, 4, 5, 6, 7], scale: 1, atlasW: 256,
              out: null, report: false, json: false, check: false, trueScale: false, file: null }
  for (let i = 0; i < argv.length; i++) {
    const a = argv[i]
    if (a === '--check') o.check = true
    else if (a === '--report') o.report = true
    else if (a === '--json') o.json = true
    else if (a === '--true-scale') o.trueScale = true
    else if (a === '--out') o.out = argv[++i]
    else if (a === '--emit-c') o.emitC = argv[++i]
    else if (a === '--tw') o.tw = parseInt(argv[++i], 10)
    else if (a === '--cw') o.cw = parseInt(argv[++i], 10)
    else if (a === '--zh') o.zh = parseInt(argv[++i], 10)
    else if (a === '--scale') o.scale = parseInt(argv[++i], 10)
    else if (a === '--atlas-w') o.atlasW = parseInt(argv[++i], 10)
    else if (a === '--rots') o.rots = argv[++i].split(',').map(s => parseInt(s.trim(), 10))
    else if (a === '-h' || a === '--help') o.help = true
    else if (!a.startsWith('-')) o.file = a
    else throw new Error(`unknown option ${a}`)
  }
  // cw defaults to tw, which makes a tile span the SAME screen width in both families, so
  // rotating between them does not appear to zoom. That is geometrically "wrong" (a square
  // turned 45° really is √2 wider across its diagonal) but iso games are not geometrically
  // truthful anyway, and matching the footprint is what makes an 8-rotation view usable.
  // --true-scale opts into the honest ratio, and into the visible pop.
  if (o.trueScale) o.cw = o.tw / Math.SQRT2
  else if (o.cw === null) o.cw = o.tw
  return o
}

function usage() {
  const src = fs.readFileSync(__filename, 'utf8').split('\n')
  console.log(src.filter(l => l.startsWith('//')).map(l => l.replace(/^\/\/ ?/, '')).join('\n'))
}

async function main() {
  let opt
  try { opt = parseArgs(process.argv.slice(2)) } catch (e) { console.error(e.message); process.exit(2) }
  if (opt.help) { usage(); return }
  if (opt.check) { process.exit(selfcheck() ? 0 : 1) }
  if (!opt.file) { usage(); process.exit(2) }

  const spec = require(path.resolve(opt.file))
  let res
  try { res = bake(spec, opt) } catch (e) { console.error(`voxel-bake: ${e.message}`); process.exit(1) }
  const A = res.index.atlas

  if (opt.json) { console.log(JSON.stringify(res.index, null, 2)); return }

  console.log(`voxel-bake: ${path.basename(opt.file)}`)
  console.log(`  tile     diagonal ${opt.tw}x${opt.tw / 2}  cardinal ${opt.cw}x${(opt.cw / 2).toFixed(1)}  height ${opt.zh}px/voxel`)
  console.log(`  rots     ${opt.rots.join(',')}`)
  for (const [name, md] of Object.entries(res.index.models)) {
    const dims = opt.rots.map(r => `${md.rots[r].w}x${md.rots[r].h}`)
    const uniq = [...new Set(dims)].join(' ')
    const pxs = opt.rots.reduce((s, r) => s + md.rots[r].w * md.rots[r].h, 0)
    console.log(`  ${name.padEnd(10)} ${md.nx}x${md.ny}x${md.nz} vox (${md.voxels})  cells ${uniq}  ${pxs}px`)
  }
  console.log(`  atlas    ${A.w}x${A.h} = ${A.pixels}px  (cells ${A.cellPixels}px, ${(100 * A.cellPixels / A.pixels).toFixed(0)}% packed)`)
  const SHEET = 128 * 128
  const verdict = A.pixels <= SHEET ? 'fits the 128x128 cart sheet'
    : `DOES NOT FIT the 128x128 cart sheet (${SHEET}px) — needs ${Math.ceil(Math.sqrt(A.pixels) / 128) * 128}px square or wider`
  console.log(`  budget   ${verdict}`)

  if (opt.out) {
    const w = await writeAtlas(res, opt.out, opt.scale)
    console.log(`  wrote    ${w.png}  +  ${path.basename(w.json)}`)
  }
  if (opt.emitC) {
    emitC(res, spec, opt, opt.emitC)
    console.log(`  wrote    ${opt.emitC}  (sig ${signature(spec, opt)})`)
  }
}

if (require.main === module) main().catch(e => { console.error(e); process.exit(1) })
module.exports = { parseModel, projector, bakeCell, bake, packAtlas, materialTones, faceTone,
                   signature, emitC, atlasPixels, parseArgs, selfcheck }
