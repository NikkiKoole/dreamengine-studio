// config for cannonfodder.c — sprites are generated in JS (flat arrays) so we can
// reach palette indices 28/29, the "magic" uniform + helmet colors that the cart
// recolors per team with pal() (the crowd.c trick). The map is built at runtime in
// C with mset() — three missions from one tileset — so no map block is needed here.
//
// Slot layout (8-col sheet):
//   0,1   — soldier: stand + mid-stride (top-down, ~11px tall)
//   8     — ground/grass     9 — water        10 — tree
//   11    — hut (target)     12 — gun nest    13 — exit flag
//
// Terrain greens (3/11/27) are recolored per biome in the cart (jungle→desert→snow),
// so trees/grass/canopies shift together; trunks, water and roofs stay put.

const { blank, pixel, rectfill, outlined, flat, OUT } = require('../sprite-draw.js')

const TRANS = 0
const UNI   = 28      // MAGIC uniform  → pal(28, teamColor)
const HEL   = 29      // MAGIC helmet   → pal(29, teamColor)
const SKIN  = 15      // light peach
const BOOT  = 4       // brown

// grass palette (recolored per biome)
const G_DK = 3, G_MD = 27, G_LT = 11

// deterministic per-pixel "hash" for texturing tiles
const hash = (x, y, a, b, m) => (x * a + y * b) % m

// ── soldier (top-down) ───────────────────────────────────────
function makeSoldier(step) {
  const g = blank()
  // helmet
  rectfill(g, 5, 3, 10, 5, HEL)
  pixel(g, 4, 4, HEL); pixel(g, 11, 4, HEL)
  // face brim peeking out the front
  rectfill(g, 6, 6, 9, 6, SKIN)
  // torso + arms (uniform)
  rectfill(g, 4, 7, 11, 11, UNI)
  rectfill(g, 3, 8, 3, 10, UNI)
  rectfill(g, 12, 8, 12, 10, UNI)
  // legs + boots, staggered for the stride
  if (step === 0) {
    rectfill(g, 5, 12, 6, 13, UNI); rectfill(g, 5, 14, 6, 14, BOOT)
    rectfill(g, 9, 12, 10, 13, UNI); rectfill(g, 9, 14, 10, 14, BOOT)
  } else {
    rectfill(g, 5, 12, 6, 14, UNI); rectfill(g, 5, 14, 6, 14, BOOT) // planted foot
    rectfill(g, 9, 12, 10, 12, UNI); rectfill(g, 9, 13, 10, 13, BOOT) // raised foot
  }
  return flat(outlined(g))
}

// ── tiles (all opaque — they include a ground base so nothing turns transparent) ──
function grass(g) {
  rectfill(g, 0, 0, 15, 15, G_DK)
  for (let y = 0; y < 16; y++)
    for (let x = 0; x < 16; x++) {
      const n = hash(x, y, 7, 13, 17)
      if (n === 0) g[y][x] = G_LT
      else if (n === 5) g[y][x] = G_MD
    }
}
function groundTile() { const g = blank(); grass(g); return flat(g) }

function waterTile() {
  const g = blank()
  rectfill(g, 0, 0, 15, 15, 1)            // dark blue
  for (let y = 0; y < 16; y++)
    for (let x = 0; x < 16; x++) {
      const n = hash(x, y, 5, 9, 11)
      if (n === 0) g[y][x] = 12       // bright blue ripple
      else if (n === 4) g[y][x] = 17  // darker deep
    }
  return flat(g)
}

function treeTile() {
  const g = blank(); grass(g)
  rectfill(g, 7, 9, 8, 13, BOOT)          // trunk
  // canopy (grass greens so it recolors with the biome)
  rectfill(g, 4, 2, 11, 7, G_DK)
  rectfill(g, 5, 1, 10, 1, G_DK)
  rectfill(g, 3, 4, 3, 6, G_DK); rectfill(g, 12, 4, 12, 6, G_DK)
  for (let y = 1; y < 8; y++)
    for (let x = 3; x < 13; x++)
      if (g[y][x] === G_DK && hash(x, y, 7, 5, 6) === 0) g[y][x] = G_LT
  return flat(g)
}

function hutTile() {
  const g = blank(); grass(g)
  rectfill(g, 3, 7, 12, 14, 4)            // walls (brown)
  rectfill(g, 3, 7, 12, 7, 20)           // shadow line under roof
  rectfill(g, 2, 3, 13, 7, 8)            // red roof
  rectfill(g, 1, 6, 14, 7, 24)           // dark roof eave
  rectfill(g, 6, 10, 9, 14, 20)          // doorway
  pixel(g, 4, 9, 0); pixel(g, 11, 9, 0) // windows
  return flat(g)
}

function nestTile() {
  const g = blank(); grass(g)
  // sandbag ring
  rectfill(g, 2, 4, 13, 13, 22)
  rectfill(g, 4, 6, 11, 11, 6)
  for (let y = 4; y <= 13; y++)
    for (let x = 2; x <= 13; x++)
      if (g[y][x] === 22 && hash(x, y, 3, 7, 4) === 0) g[y][x] = 20
  rectfill(g, 6, 7, 9, 10, 5)            // dark emplacement
  rectfill(g, 9, 8, 13, 9, 5)            // the gun barrel poking out
  return flat(g)
}

function exitTile() {
  const g = blank(); grass(g)
  rectfill(g, 0, 0, 15, 15, G_DK)
  // a marked landing zone + flag
  rectfill(g, 2, 2, 13, 13, G_MD)
  for (let i = 0; i < 16; i++) { pixel(g, i, 0, 10); pixel(g, i, 15, 10) } // edge stripe
  rectfill(g, 7, 4, 7, 13, 6)           // flag pole
  rectfill(g, 8, 4, 12, 7, 10)          // yellow flag
  pixel(g, 8, 5, 9); pixel(g, 11, 6, 9)
  return flat(g)
}

module.exports = {
  screenW: 320, screenH: 200, scale: 4,
  cellW: 16, cellH: 16,
  sprites: {
    0:  makeSoldier(0),
    1:  makeSoldier(1),
    8:  groundTile(),
    9:  waterTile(),
    10: treeTile(),
    11: hutTile(),
    12: nestTile(),
    13: exitTile(),
  },
}
