// config for turfwar.c — sprites + city map.
//
// Members are tiny top-down people. Their SHIRT pixels are drawn in the "magic"
// palette index 29; turfwar.c calls pal(29, gangColor) before drawing each gang
// so one sprite set yields four differently-colored crews (the crowd.c idiom).
// Two near-identical frames give a subtle walk bob.
//
// Slot layout (8-col sheet):
//   0,1 — member walk frames A/B (magic shirt = 29)
//   4   — city block / building tile (opaque, 16×16) used by the map
//
// Sprites are raw index arrays (generated here) so we can use palette index 29,
// which is past the ASCII char map's 0–15 range.

const { blank, pixel, rectfill, outlined, flat, OUT } = require('../sprite-draw.js')

const TRANS = 0    // transparent (cart calls colorkey(0))
const SKIN  = 15   // light peach
const HAIR  = 16
const SHOE  = 16
const SHIRT = 29   // MAGIC: recolored per gang via pal(29, ...)

// top-down person; `sway` flips the arm/foot offsets for the walk frame
function person(sway) {
  const g = blank()
  rectfill(g, 4, 6, 11, 12, SHIRT)             // shoulders / torso
  pixel(g, 4, 6, TRANS); pixel(g, 11, 6, TRANS)   // round the top corners
  pixel(g, 4, 12, TRANS); pixel(g, 11, 12, TRANS) // round the bottom corners
  if (!sway) { rectfill(g, 3, 8, 3, 10, SHIRT); rectfill(g, 12, 9, 12, 11, SHIRT) }   // arms
  else       { rectfill(g, 3, 9, 3, 11, SHIRT); rectfill(g, 12, 8, 12, 10, SHIRT) }
  rectfill(g, 6, 3, 9, 7, SKIN)                // head
  pixel(g, 6, 3, TRANS); pixel(g, 9, 3, TRANS)
  rectfill(g, 6, 2, 9, 3, HAIR)                // hair cap
  if (!sway) { pixel(g, 6, 13, SHOE); pixel(g, 9, 14, SHOE) }   // feet
  else       { pixel(g, 6, 14, SHOE); pixel(g, 9, 13, SHOE) }
  return flat(outlined(g))
}

// top-down city block — dark roof, lit top/left edges, a couple of rooftop units
function building() {
  const g = blank()
  rectfill(g, 0, 0, 15, 15, 5)        // dark grey roof
  rectfill(g, 0, 0, 15, 0, 6)         // lit north edge
  rectfill(g, 0, 0, 0, 15, 6)         // lit west edge
  rectfill(g, 0, 15, 15, 15, 16)      // shadow south edge
  rectfill(g, 15, 0, 15, 15, 16)      // shadow east edge
  rectfill(g, 4, 4, 7, 7, 22)         // rooftop unit
  rectfill(g, 9, 9, 12, 12, 21)       // rooftop unit
  pixel(g, 5, 5, 7)                // vent glints
  pixel(g, 11, 11, 7)
  return flat(g)
}

// ── city map: REG_COLS×REG_ROWS districts, each RW×RH cells, building in the middle
const REG_COLS = 4, REG_ROWS = 3, RW = 7, RH = 5
const MW = REG_COLS * RW, MH = REG_ROWS * RH
const layout = []
for (let y = 0; y < MH; y++) {
  let row = ''
  for (let x = 0; x < MW; x++) {
    const lx = x % RW, ly = y % RH
    const wall = (lx >= 2 && lx <= 4 && ly >= 1 && ly <= 3)   // centered 3×3 block; open streets around
    row += wall ? 'B' : '.'
  }
  layout.push(row)
}

module.exports = {
  screenW: 320, screenH: 200, scale: 4,
  cellW: 16, cellH: 16, mapW: MW, mapH: MH,
  sprites: {
    0: person(false),
    1: person(true),
    4: building(),
  },
  map: {
    layout,
    tiles: { B: 4 },
  },
}
