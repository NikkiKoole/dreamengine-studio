// font-bake.js — bake text from a real TTF (any Google Font) into sprite pixels
// at BUILD time. No runtime cost: the rasterized text lands in the cart's
// sprite sheet like any hand-drawn sprite.
//
//   const { bakeText } = require('../font-bake.js')
//   const g = bakeText('fonts/Bungee-Regular.ttf', 'DREAM', { px: 28, color: 9 })
//   // g is a 2D palette-index grid — compose with sprite-draw.js
//   // (stamp it onto a blank() canvas, outline it, split() it into slots)
//
// Font paths are resolved relative to tools/ unless absolute. Drop new fonts
// into tools/fonts/ — github.com/google/fonts has raw TTFs for every family:
//   curl -sL -o tools/fonts/<File>.ttf \
//     "https://github.com/google/fonts/raw/main/ofl/<family>/<File>.ttf"
//
// How it works: vendor/opentype.cjs parses the TTF and gives glyph outlines
// (with kerning); we flatten the béziers to polylines and scanline-fill them
// with the nonzero winding rule, 3×3 supersampled. Coverage >= threshold
// becomes `color`; partial coverage optionally becomes `aa` (a darker edge
// color) so small text doesn't look chewed.

const fs = require('fs')
const path = require('path')
const opentype = require('./vendor/opentype.cjs')
const { blank, clone, stamp, split, outlined } = require('./sprite-draw.js')

const fontCache = {}

// Parse (and cache) a TTF. Relative paths resolve against tools/.
function loadFont(fontPath) {
  const p = path.isAbsolute(fontPath) ? fontPath : path.join(__dirname, fontPath)
  if (!fontCache[p]) {
    const buf = fs.readFileSync(p)
    fontCache[p] = opentype.parse(buf.buffer.slice(buf.byteOffset, buf.byteOffset + buf.byteLength))
  }
  return fontCache[p]
}

// Pixel bounds of `text` at font size `px` — cheap (outline bbox, no raster).
// Use to pick a px that fits a slot budget before calling bakeText.
function measure(fontPath, text, px) {
  const b = loadFont(fontPath).getPath(text, 0, 0, px).getBoundingBox()
  return { w: Math.ceil(b.x2 - b.x1), h: Math.ceil(b.y2 - b.y1) }
}

// Flatten a path's M/L/Q/C/Z commands into closed polyline contours.
function contoursFromPath(p) {
  const contours = []
  let cur = null, sx = 0, sy = 0, x = 0, y = 0
  for (const c of p.commands) {
    if (c.type === 'M') { cur = [[c.x, c.y]]; contours.push(cur); sx = x = c.x; sy = y = c.y }
    else if (c.type === 'L') { cur.push([c.x, c.y]); x = c.x; y = c.y }
    else if (c.type === 'Q') {
      for (let i = 1; i <= 10; i++) {
        const t = i / 10, u = 1 - t
        cur.push([u * u * x + 2 * u * t * c.x1 + t * t * c.x,
                  u * u * y + 2 * u * t * c.y1 + t * t * c.y])
      }
      x = c.x; y = c.y
    } else if (c.type === 'C') {
      for (let i = 1; i <= 12; i++) {
        const t = i / 12, u = 1 - t
        cur.push([u * u * u * x + 3 * u * u * t * c.x1 + 3 * u * t * t * c.x2 + t * t * t * c.x,
                  u * u * u * y + 3 * u * u * t * c.y1 + 3 * u * t * t * c.y2 + t * t * t * c.y])
      }
      x = c.x; y = c.y
    } else if (c.type === 'Z' && cur) cur.push([sx, sy])
  }
  return contours
}

// Scanline coverage of the contours over a w×h pixel grid (nonzero winding,
// S×S samples per pixel). Returns Float32Array of 0..1 coverage per pixel.
function rasterize(contours, w, h, S = 3) {
  const cov = new Float32Array(w * h)
  const inc = 1 / (S * S)
  for (let sy = 0; sy < h * S; sy++) {
    const y = (sy + 0.5) / S
    const xs = []
    for (const c of contours)
      for (let i = 0; i < c.length - 1; i++) {
        const y1 = c[i][1], y2 = c[i + 1][1]
        if ((y1 <= y) === (y2 <= y)) continue            // segment doesn't cross this row
        const t = (y - y1) / (y2 - y1)
        xs.push([c[i][0] + t * (c[i + 1][0] - c[i][0]), y2 > y1 ? 1 : -1])
      }
    if (!xs.length) continue
    xs.sort((a, b) => a[0] - b[0])
    let k = 0, wind = 0
    const py = (sy / S) | 0
    for (let sxs = 0; sxs < w * S; sxs++) {
      const xpt = (sxs + 0.5) / S
      while (k < xs.length && xs[k][0] <= xpt) wind += xs[k++][1]
      if (wind !== 0) cov[py * w + ((sxs / S) | 0)] += inc
    }
  }
  return cov
}

// Rasterize `text` to a tight 2D palette-index grid (0 = transparent).
//   px        font size in pixels
//   color     palette index for solid pixels
//   aa        optional palette index for partially-covered edge pixels (-1 = off);
//             pick a darker neighbour of `color` for a soft edge
//   threshold coverage needed for a solid pixel (default 0.5)
//   pad       transparent border in pixels (default 1)
function bakeText(fontPath, text, { px = 16, color = 7, aa = -1, threshold = 0.5, pad = 1 } = {}) {
  const font = loadFont(fontPath)
  const b = font.getPath(text, 0, 0, px).getBoundingBox()
  const w = Math.ceil(b.x2 - b.x1) + pad * 2
  const h = Math.ceil(b.y2 - b.y1) + pad * 2
  const p = font.getPath(text, pad - b.x1, pad - b.y1, px)
  const cov = rasterize(contoursFromPath(p), w, h)
  const g = []
  for (let y = 0; y < h; y++) {
    const row = []
    for (let x = 0; x < w; x++) {
      const c = cov[y * w + x]
      row.push(c >= threshold ? color : (aa >= 0 && c >= threshold * 0.5 ? aa : 0))
    }
    g.push(row)
  }
  return g
}

// Bake `text` centered into a slotCols×slotRows slot-rect at the biggest px
// that fits, returning flat 16×16 tiles (row-major) ready for sprite slots.
// Place each row of tiles 8 slots apart so the sheet region is contiguous and
// the C side can sspr() one constant rect regardless of the word's width.
// Treatments compose like on any sprite:
//   color/aa  see bakeText
//   outline   sprite-draw outlined() border color (-1 = off)
//   shadow    1px drop shadow color, stamped under the (outlined) text (-1 = off)
function bakeBanner(fontPath, text, slotCols, slotRows, { color = 7, aa = -1, outline = -1, shadow = -1 } = {}) {
  const trim = 3 + (outline >= 0 ? 2 : 0) + (shadow >= 0 ? 1 : 0)
  const maxW = slotCols * 16 - trim, maxH = slotRows * 16 - trim
  let px = 64
  while (px > 6) {
    const m = measure(fontPath, text, px)
    if (m.w <= maxW && m.h <= maxH) break
    px--
  }
  let g = bakeText(fontPath, text, { px, color, aa, pad: outline >= 0 ? 2 : 1 })
  if (outline >= 0) g = outlined(g, outline)
  const c = blank(slotCols * 16, slotRows * 16)
  const ox = ((slotCols * 16 - g[0].length) / 2) | 0
  const oy = ((slotRows * 16 - g.length) / 2) | 0
  if (shadow >= 0) {
    const sh = clone(g)
    for (const row of sh) for (let i = 0; i < row.length; i++) if (row[i]) row[i] = shadow
    stamp(c, sh, ox + 1, oy + 1)
  }
  stamp(c, g, ox, oy)
  return split(c)
}

module.exports = { loadFont, measure, bakeText, bakeBanner }
