// sprite-draw.js — reusable 2D pixel-canvas API for .cart.js sprite authoring.
//
// All operations work on a 2D array: g[y][x] = paletteIndex (0–31).
// Matches dreamengine's C drawing API names where possible.
//
// Usage in a .cart.js file:
//   const { blank, pixel, rectfill, line, circlefill, trifill,
//           outlined, mirror, stamp, flat, split, OUT } = require('../sprite-draw.js')
//
// Typical workflow:
//   function mySprite() {
//     const g = blank()           // 16×16 canvas, all zeros (transparent)
//     rectfill(g, 2, 4, 13, 12, 28)   // filled rect in magic body color
//     pixel(g, 7, 5, 15)              // single pixel
//     return flat(outlined(g))        // outline + export
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

// Split a wide 2D canvas into an array of flat slotW-wide column strips.
// Returns [leftSlot, rightSlot] for a 32-wide canvas, four slots for 32×32, etc.
// Usage (32-wide weapon spanning two slots):
//   const [idleL, idleR] = split(pistolGrid(false))
//   sprites: { 16: idleL, 17: idleR }
function split(g, slotW = 16) {
  const cols = g[0].length / slotW
  return Array.from({ length: cols }, (_, col) =>
    g.flatMap(row => row.slice(col * slotW, (col + 1) * slotW))
  )
}

module.exports = { blank, pixel, rectfill, line, circlefill, trifill, outlined, mirror, stamp, flat, split, OUT }
