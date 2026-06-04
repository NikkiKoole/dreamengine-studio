// config for sensi.c — Sensible Soccer.
//
// Sprites are generated here so we can paint the "magic" kit colours in palette
// indices 28 (SHIRT) and 29 (SHORTS) — placeholders the cart swaps per team with
// pal(28,..) / pal(29,..), exactly like the crowd cart. One tiny top-down player
// → two outfield kits + two keepers, all from four run frames.
//
// Slot layout (8-col sheet):
//   0,1,2,3  — top-down player, a 4-step run cycle [neutral, L-stride, neutral, R-stride]
//   8,9      — pitch tiles: dark mow stripe, light mow stripe

const { blank, pixel, rectfill, outlined, flat, OUT } = require('../sprite-draw.js')

// palette indices
const TRANS = 0    // transparent (cart calls colorkey(0))
const SKIN  = 15   // light peach
const HAIR  = 4    // brown
const BOOT  = 5    // dark grey
const SHIRT = 28   // MAGIC: pal(28, teamShirt)
const SHORTS= 29   // MAGIC: pal(29, teamShorts)

// ── top-down footballer, run cycle by stride 0..3 ────────────────────────────
function makePlayer(stride) {
  const g = blank()
  // head + hair
  rectfill(g, 6, 4, 9, 5, SKIN)
  rectfill(g, 6, 3, 9, 3, HAIR)
  // shoulders + shirt body, short sleeves to the sides
  rectfill(g, 5, 6, 10, 9, SHIRT)
  rectfill(g, 4, 6, 4, 8, SHIRT)
  rectfill(g, 11, 6, 11, 8, SHIRT)
  pixel(g, 4, 9, SKIN); pixel(g, 11, 9, SKIN)   // hands
  // shorts
  rectfill(g, 5, 10, 10, 11, SHORTS)
  // legs — stride lengthens one leg, shortens the other (subtle 1px march)
  const ll = stride === 1 ? 14 : stride === 3 ? 12 : 13
  const rl = stride === 3 ? 14 : stride === 1 ? 12 : 13
  rectfill(g, 6, 12, 7, ll, SKIN); rectfill(g, 6, ll, 7, ll, BOOT)
  rectfill(g, 9, 12, 10, rl, SKIN); rectfill(g, 9, rl, 10, rl, BOOT)
  return flat(outlined(g))
}

// ── pitch tiles: dark + light mow stripes (fully opaque, avoid index 0) ───────
function darkStripe() {
  const g = blank()
  rectfill(g, 0, 0, 15, 15, 3)                    // dark green base
  for (let y = 0; y < 16; y++)
    for (let x = 0; x < 16; x++)
      if ((x * 7 + y * 13) % 23 === 0) g[y][x] = 27  // sparse blade fleck
  return flat(g)
}
function lightStripe() {
  const g = blank()
  rectfill(g, 0, 0, 15, 15, 3)
  for (let y = 0; y < 16; y++)                // checker of lighter green → "mowed" band
    for (let x = 0; x < 16; x++)
      if ((x + y) % 2 === 0) g[y][x] = 27
  return flat(g)
}

// ── map: 20 × 40 cells, mow stripes in 3-row bands ────────────────────────────
const MW = 20, MH = 40
const layout = []
for (let y = 0; y < MH; y++) {
  const c = (Math.floor(y / 3) % 2 === 0) ? 'a' : 'b'
  layout.push(c.repeat(MW))
}

module.exports = {
  screenW: 320, screenH: 200, scale: 4,
  cellW: 16, cellH: 16, mapW: MW, mapH: MH,
  sprites: {
    0: makePlayer(0),
    1: makePlayer(1),
    2: makePlayer(2),
    3: makePlayer(3),
    8: darkStripe(),
    9: lightStripe(),
  },
  map: {
    layout,
    tiles: { a: 8, b: 9 },
  },
}
