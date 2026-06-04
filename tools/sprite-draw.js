// sprite-draw.js — reusable 2D pixel-canvas API for .cart.js sprite authoring.
//
// All operations work on a 2D array: g[y][x] = paletteIndex (0–31).
// Matches dreamengine's C drawing API names where possible.
//
// Usage in a .cart.js file:
//   const { blank, pixel, rectfill, line, circlefill, ovalfill, trifill,
//           rrectfill, ngonfill, polyfill, noise,
//           shade, rotate, rotations, scale2x, replace, clone,
//           outlined, mirror, stamp, flat, split, OUT } = require('../sprite-draw.js')
//
// Typical workflow:
//   function mySprite() {
//     const g = blank()                    // 16×16 canvas, all zeros (transparent)
//     ovalfill(g, 8, 8, 6, 4, 28)          // ellipse body in magic body color
//     pixel(g, 7, 5, 15)                   // single pixel
//     return flat(outlined(g))             // outline + export
//   }
//   module.exports = { sprites: { 0: mySprite() } }
//
// When to use programmatic sprites (vs ASCII art strings):
//   - You need palette indices 16–31 (magic recolor targets, extended darks)
//   - The sprite has geometric structure (circles, triangles, mirrored craft)
//   - The ASCII charMap's 0–15 range is too limiting

'use strict'

// Default outline color: CLR_BROWNISH_BLACK (index 16).
// Reads near-black against any background; never 0 so it's never transparent.
const OUT = 16

// ── canvas creation ───────────────────────────────────────────────────────────

// Create a blank w×h canvas filled with `fill` (default 0 = transparent).
// Default 16×16 matches one sprite slot. Use blank(32, 16) for a two-slot-wide
// weapon/banner, blank(32, 32) for a 2×2-slot boss.
function blank(w = 16, h = 16, fill = 0) {
  return Array.from({ length: h }, () => new Array(w).fill(fill))
}

// ── drawing primitives (all bounds-checked; out-of-range pixels are ignored) ──

function pixel(g, x, y, c) {
  x = Math.round(x); y = Math.round(y)
  if (x >= 0 && x < g[0].length && y >= 0 && y < g.length) g[y][x] = c
}

function rectfill(g, x0, y0, x1, y1, c) {
  for (let y = y0; y <= y1; y++)
    for (let x = x0; x <= x1; x++)
      pixel(g, x, y, c)
}

// Rounded rectangle. r = corner radius; clamped to fit.
// Matches studio.h's rrectfill(x, y, w, h, r, color) signature.
// Great for panels, speech bubbles, and UI elements baked into sprites.
function rrectfill(g, x, y, w, h, r, c) {
  r = Math.max(0, Math.min(r, Math.floor(w / 2), Math.floor(h / 2)))
  const x1 = x + r, x2 = x + w - 1 - r
  const y1 = y + r, y2 = y + h - 1 - r
  for (let py = y; py < y + h; py++) {
    for (let px = x; px < x + w; px++) {
      // pixels not in a corner zone are always inside
      if ((px >= x1 && px <= x2) || (py >= y1 && py <= y2)) { pixel(g, px, py, c); continue }
      // corner zone: pixel center vs nearest corner center
      const cx = Math.max(x1, Math.min(px, x2))
      const cy = Math.max(y1, Math.min(py, y2))
      const ddx = px + 0.5 - cx, ddy = py + 0.5 - cy
      if (ddx * ddx + ddy * ddy <= r * r) pixel(g, px, py, c)
    }
  }
}

// Bresenham line, inclusive endpoints.
function line(g, x0, y0, x1, y1, c) {
  x0 = Math.round(x0); y0 = Math.round(y0)
  x1 = Math.round(x1); y1 = Math.round(y1)
  const dx = Math.abs(x1 - x0), dy = Math.abs(y1 - y0)
  const sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1
  let err = dx - dy
  for (;;) {
    pixel(g, x0, y0, c)
    if (x0 === x1 && y0 === y1) break
    const e2 = 2 * err
    if (e2 > -dy) { err -= dy; x0 += sx }
    if (e2 <  dx) { err += dx; y0 += sy }
  }
}

// Filled circle. Uses pixel-center sampling (x+0.5, y+0.5) for clean edges.
function circlefill(g, cx, cy, r, c) {
  for (let y = 0; y < g.length; y++)
    for (let x = 0; x < g[0].length; x++)
      if ((x + 0.5 - cx) ** 2 + (y + 0.5 - cy) ** 2 <= r * r) g[y][x] = c
}

// Filled ellipse. rx = horizontal radius, ry = vertical radius.
// Matches studio.h's ovalfill(x, y, rx, ry, color) signature.
// Use for heads, eyes, planet shapes, bodies — perfect circles are rarely enough.
function ovalfill(g, cx, cy, rx, ry, c) {
  if (rx <= 0 || ry <= 0) return
  for (let y = 0; y < g.length; y++)
    for (let x = 0; x < g[0].length; x++) {
      const dx = x + 0.5 - cx, dy = y + 0.5 - cy
      if ((dx * dx) / (rx * rx) + (dy * dy) / (ry * ry) <= 1) g[y][x] = c
    }
}

// Filled triangle via barycentric coverage. Pixel-center sampling.
function trifill(g, x1, y1, x2, y2, x3, y3, c) {
  for (let y = 0; y < g.length; y++) {
    for (let x = 0; x < g[0].length; x++) {
      const cx = x + 0.5, cy = y + 0.5
      const w1 = (x2 - cx) * (y3 - cy) - (x3 - cx) * (y2 - cy)
      const w2 = (x3 - cx) * (y1 - cy) - (x1 - cx) * (y3 - cy)
      const w3 = (x1 - cx) * (y2 - cy) - (x2 - cx) * (y1 - cy)
      if ((w1 >= 0 && w2 >= 0 && w3 >= 0) || (w1 <= 0 && w2 <= 0 && w3 <= 0)) g[y][x] = c
    }
  }
}

// Filled arbitrary polygon via even-odd scanline coverage.
// pts = flat array [x0,y0, x1,y1, ...], matches C's polyfill(int *xy, n, color).
// Handles concave shapes. Great for lightning bolts, chevrons, custom silhouettes.
function polyfill(g, pts, c) {
  const n = pts.length >> 1
  if (n < 3) return
  let minY = Infinity, maxY = -Infinity
  for (let i = 0; i < n; i++) { minY = Math.min(minY, pts[i*2+1]); maxY = Math.max(maxY, pts[i*2+1]) }
  for (let py = Math.floor(minY); py <= Math.ceil(maxY); py++) {
    const sy = py + 0.5   // pixel center
    const xs = []
    for (let i = 0; i < n; i++) {
      const j = (i + 1) % n
      const ax = pts[i*2], ay = pts[i*2+1], bx = pts[j*2], by = pts[j*2+1]
      if ((ay <= sy && by > sy) || (by <= sy && ay > sy))
        xs.push(ax + (sy - ay) * (bx - ax) / (by - ay))
    }
    xs.sort((a, b) => a - b)
    for (let i = 0; i < xs.length - 1; i += 2) {
      const x0 = Math.round(xs[i]), x1 = Math.round(xs[i + 1]) - 1
      for (let x = x0; x <= x1; x++) pixel(g, x, py, c)
    }
  }
}

// Filled regular n-sided polygon (triangle=3, square=4, hex=6, octagon=8…).
// rot=0 puts the first vertex directly right; rot=90 puts it down.
// Matches studio.h's ngonfill(x, y, r, sides, rot, color) signature.
function ngonfill(g, x, y, r, sides, rot, c) {
  const pts = []
  const base = (rot || 0) * Math.PI / 180
  for (let i = 0; i < sides; i++) {
    const a = base + i * 2 * Math.PI / sides
    pts.push(x + r * Math.cos(a), y + r * Math.sin(a))
  }
  polyfill(g, pts, c)
}

// ── texture noise ─────────────────────────────────────────────────────────────

// Deterministic per-pixel integer hash — returns a value in [0, mod).
// The terrain-tile idiom: `if (noise(x, y, 19) === 0) g[y][x] = fleckColor`
// paints a sparse, stable speckle pattern without copy-pasting LCG constants.
// Different seeds naturally emerge from different mod/usage combos; for more
// variety call twice with swapped axes: `noise(y, x, 11)`.
function noise(x, y, mod) {
  // FNV-1a hash — good distribution even in small 16×16 grids
  let h = 0x811c9dc5
  h = Math.imul(h ^ (x & 0xff), 0x01000193)
  h = Math.imul(h ^ (y & 0xff), 0x01000193)
  h = h >>> 0
  return mod !== undefined ? h % mod : h
}

// ── palette ramps ─────────────────────────────────────────────────────────────
// THE curated light/dark LUT for the fixed 32-color palette: RAMP_DARKER[c] is
// one shading step darker than c (same hue family), RAMP_LIGHTER[c] one step
// lighter. Used by shade(); exported because anything that auto-shades pixel
// art against this palette needs the same table.
//                       0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31
const RAMP_DARKER  = [   0,17,18,19,20,21,13, 6,24, 4, 9, 3, 1, 5, 2,31, 0, 0,16,17,16,18, 5,10, 2, 4,11, 3, 1,18, 4,30]
const RAMP_LIGHTER = [   5,13,24,11, 9,22, 7, 7,14,10,23,26, 6, 6,15, 7,20, 1, 2, 3, 4, 5, 6, 7, 8, 9,23,11,12,13,31,15]

// ── post-processing ───────────────────────────────────────────────────────────

// Trace a 1px outline into every transparent pixel (value `transparent`) that
// touches a non-transparent, non-outline pixel. Works on any canvas width/height.
// Call before flat() for sprites that need a clean dark border.
function outlined(g, outColor = OUT, transparent = 0) {
  const h = g.length, w = g[0].length
  const o = g.map(r => r.slice())
  for (let y = 0; y < h; y++) {
    for (let x = 0; x < w; x++) {
      if (g[y][x] !== transparent) continue
      let touch = false
      for (let dy = -1; dy <= 1 && !touch; dy++) {
        for (let dx = -1; dx <= 1; dx++) {
          const ny = y + dy, nx = x + dx
          if (nx >= 0 && nx < w && ny >= 0 && ny < h &&
              g[ny][nx] !== transparent && g[ny][nx] !== outColor) { touch = true; break }
        }
      }
      if (touch) o[y][x] = outColor
    }
  }
  return o
}

// Auto light/shadow: every filled pixel whose neighbor TOWARD the light is
// transparent gets one ramp step lighter; whose neighbor AWAY from it is
// transparent gets one step darker. Turns flat fills into lit volumes in one
// call. lightDeg uses the engine's y-down convention (0 = light from the
// right, 90 = from below); default 225 = classic top-left light.
// Call BEFORE outlined() — shade only reads silhouette edges vs `transparent`.
function shade(g, lightDeg = 225, transparent = 0) {
  const h = g.length, w = g[0].length
  const rad = lightDeg * Math.PI / 180
  const lx = Math.round(Math.cos(rad)), ly = Math.round(Math.sin(rad))
  const at = (x, y) => (x < 0 || x >= w || y < 0 || y >= h) ? transparent : g[y][x]
  const o = g.map(r => r.slice())
  for (let y = 0; y < h; y++)
    for (let x = 0; x < w; x++) {
      const c = g[y][x]
      if (c === transparent) continue
      if      (at(x + lx, y + ly) === transparent) o[y][x] = RAMP_LIGHTER[c]
      else if (at(x - lx, y - ly) === transparent) o[y][x] = RAMP_DARKER[c]
    }
  return o
}

// Rotate a canvas `deg` degrees clockwise around its center (nearest-neighbor,
// inverse-mapped: every dest pixel samples back into the source, so no holes).
// Returns a new canvas the same size; corners that swing outside become
// `transparent`. Unlike runtime spr_rot(), a BAKED rotation can still be
// outlined()/shade()d afterwards — rotate first, post-process after.
function rotate(g, deg, transparent = 0) {
  const h = g.length, w = g[0].length
  const cx = w / 2, cy = h / 2
  const rad = -deg * Math.PI / 180
  const cos = Math.cos(rad), sin = Math.sin(rad)
  const o = blank(w, h, transparent)
  for (let y = 0; y < h; y++)
    for (let x = 0; x < w; x++) {
      const dx = x + 0.5 - cx, dy = y + 0.5 - cy
      const sx = Math.floor(cx + dx * cos - dy * sin)
      const sy = Math.floor(cy + dx * sin + dy * cos)
      if (sx >= 0 && sx < w && sy >= 0 && sy < h) o[y][x] = g[sy][sx]
    }
  return o
}

// n pre-baked headings of a canvas: [0°, 360/n°, …]. The top-down car/ship
// idiom — draw the craft ONCE pointing right, bake every direction:
//   rotations(ship, 8).forEach((r, i) => sprites[16 + i] = flat(outlined(r)))
function rotations(g, n) {
  return Array.from({ length: n }, (_, i) => rotate(g, i * 360 / n))
}

// EPX/Scale2x upscale: w×h → 2w×2h with diagonals smoothed (real EPX, not
// blocky doubling). The "draw small, bake big" workflow for multi-slot bosses:
// sketch at 16×16 where it's easy to reason, then scale2x() + split() → 4 slots.
function scale2x(g) {
  const h = g.length, w = g[0].length
  const at = (x, y) => g[Math.min(h - 1, Math.max(0, y))][Math.min(w - 1, Math.max(0, x))]
  const o = blank(w * 2, h * 2)
  for (let y = 0; y < h; y++)
    for (let x = 0; x < w; x++) {
      const P = g[y][x]
      const A = at(x, y - 1), B = at(x + 1, y), C = at(x - 1, y), D = at(x, y + 1)
      o[y * 2][x * 2]         = (C === A && C !== D && A !== B) ? A : P
      o[y * 2][x * 2 + 1]     = (A === B && A !== C && B !== D) ? B : P
      o[y * 2 + 1][x * 2]     = (D === C && D !== B && C !== A) ? C : P
      o[y * 2 + 1][x * 2 + 1] = (B === D && B !== A && D !== C) ? D : P
    }
  return o
}

// Replace every `from` pixel with `to`, in place. Bake-time recolor for
// variants (autumn vs spring tree) — the magic pal() indices stay the
// RUNTIME recolor tool; replace() is for when you want both baked.
function replace(g, from, to) {
  for (const row of g)
    for (let x = 0; x < row.length; x++)
      if (row[x] === from) row[x] = to
  return g
}

// Deep-copy a canvas — take one before in-place ops (replace, mirror, stamp)
// when you still need the original for another variant.
function clone(g) { return g.map(r => r.slice()) }

// Mirror the left half of a canvas onto its right half in-place.
// Works on any width — useful for symmetric sprites (aircraft, creatures).
function mirror(g) {
  const w = g[0].length
  for (let y = 0; y < g.length; y++)
    for (let x = 0; x < Math.floor(w / 2); x++)
      g[y][w - 1 - x] = g[y][x]
  return g
}

// Copy `src` onto `dst` at pixel offset (ox, oy), skipping transparent pixels.
// Useful for compositing sub-parts (a head drawn separately, then stamped onto a body).
function stamp(dst, src, ox, oy, transparent = 0) {
  for (let y = 0; y < src.length; y++)
    for (let x = 0; x < src[0].length; x++)
      if (src[y][x] !== transparent) pixel(dst, ox + x, oy + y, src[y][x])
}

// ── export ────────────────────────────────────────────────────────────────────

// Flatten a 2D canvas to a 1D palette-index array for use as a sprite slot value.
function flat(g) { return g.flat() }

// Split a big 2D canvas into flat 16×16 slot tiles, row-major.
// Returns [left, right] for a 32×16 canvas, [TL, TR, BL, BR] for 32×32, etc.
// Remember the sheet is 8 slots wide: place a 32×32's tiles at s, s+1, s+8,
// s+9 so a single sspr() can read the region back contiguously.
// Usage (32-wide weapon spanning two slots):
//   const [idleL, idleR] = split(pistolGrid(false))
//   sprites: { 16: idleL, 17: idleR }
function split(g, slotW = 16, slotH = 16) {
  const cols = g[0].length / slotW
  const rows = g.length / slotH
  const out = []
  for (let r = 0; r < rows; r++)
    for (let c = 0; c < cols; c++) {
      const tile = []
      for (let y = 0; y < slotH; y++)
        tile.push(...g[r * slotH + y].slice(c * slotW, (c + 1) * slotW))
      out.push(tile)
    }
  return out
}

module.exports = {
  blank, pixel, rectfill, rrectfill, line,
  circlefill, ovalfill, trifill, polyfill, ngonfill,
  noise,
  shade, rotate, rotations, scale2x, replace, clone,
  outlined, mirror, stamp, flat, split,
  OUT, RAMP_DARKER, RAMP_LIGHTER,
}
