// extrudetext.cart.js — baked display words for the TTF mode of the extrude
// lab. Each word is baked from the chunky "Bungee" TTF (tools/font-bake.js) as
// a FLAT single-ink silhouette on a transparent (palette 0) background, so the
// cart can pal()-recolour each extrude layer (the colourstep) and colorkey(0)
// the bg away. The other mode (bitmap print_scaled) needs no sprites.
//
// One ink index (7) for every word; the cart swaps 7 -> the layer colour.
// Sheet layout (8-col sheet, so slot = base + i):
//   "DREAM"   8x3  base 0   -> region ( 0,  0) 128x48
//   "HOTLINE" 8x3  base 24  -> region ( 0, 48) 128x48
//   "1985"    8x2  base 48  -> region ( 0, 96) 128x32

const { bakeBanner } = require('../font-bake.js')

const FONT = 'fonts/Bungee-Regular.ttf'
const INK  = 7                       // the flat silhouette colour the cart recolours

const words = [
  // [text, cols, rows, base]
  ['DREAM',   8, 3, 0],
  ['HOTLINE', 8, 3, 24],
  ['1985',    8, 2, 48],
]

const sprites = {}
for (const [text, cols, rows, base] of words)
  bakeBanner(FONT, text, cols, rows, { color: INK }).forEach((t, i) => { sprites[base + i] = t })

module.exports = { sprites }
