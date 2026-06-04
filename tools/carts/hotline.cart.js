// config for hotline.c — a top-down neon murder-floor.
//
// One body sprite (slot 0) drawn from above, facing RIGHT (+x), so the cart can
// spr_rot() it to any aim angle. Its shirt is painted in the "magic" index 28 and
// recolored per entity with pal() — white player, red guards, orange thugs — the
// crowd.c trick. Tiles 1-5 are the floor / wall / closed door / open door / exit,
// generated as flat index arrays so we can reach the dark extended palette
// (16-31) for the moody neon look. The cart pal()-tints the whole tileset per
// floor (magenta → cyan → red).

const { blank, pixel, rectfill, circlefill, outlined, flat, OUT } = require('../sprite-draw.js')

// palette indices
const TRANS  = 0
const OUTL   = 16   // brownish-black outline (reads near-black, never index 0)
const SKIN   = 31   // peach
const SHIRT  = 28   // MAGIC — recolored by pal(28, color)

// ── top-down body, facing right ──────────────────────────────────
function makeGuy() {
  const g = blank()
  circlefill(g, 7, 8, 5, SHIRT)        // torso / shoulders
  rectfill(g, 11, 7, 14, 9, SHIRT)    // arm reaching forward (toward +x)
  circlefill(g, 8, 8, 3, SKIN)         // head (seen from above)
  pixel(g, 14, 8, SKIN)            // forward hand
  return flat(outlined(g))
}

// ── tiles (16×16, fully opaque) ──────────────────────────────────
function floorTile() {
  const g = blank()
  rectfill(g, 0, 0, 15, 15, 18)       // darker purple base
  for (let y = 0; y < 16; y++)
    for (let x = 0; x < 16; x++) {
      const n = (x * 7 + y * 13) % 19
      if (n === 0) g[y][x] = 2    // dark purple speck
      else if (n === 6) g[y][x] = 17 // darker blue speck
    }
  // faint tile grid lines
  for (let i = 0; i < 16; i++) { if (i % 8 === 0) { g[0][i] = 2; g[i][0] = 2 } }
  return flat(g)
}
function wallTile() {
  const g = blank()
  rectfill(g, 0, 0, 15, 15, 2)        // dark purple
  rectfill(g, 0, 0, 15, 1, 14)        // hot pink neon strip (top)
  rectfill(g, 0, 14, 15, 15, 18)      // shaded bottom
  rectfill(g, 0, 0, 0, 15, 29)        // mauve left edge
  return flat(g)
}
function doorTile() {
  const g = blank()
  rectfill(g, 0, 0, 15, 15, 24)       // dark red slab
  rectfill(g, 1, 1, 14, 14, 8)        // red face
  rectfill(g, 7, 1, 8, 14, 24)        // center seam
  rectfill(g, 0, 0, 15, 0, 16)        // top frame
  rectfill(g, 0, 15, 15, 15, 16)      // bottom frame
  pixel(g, 11, 8, 10); pixel(g, 11, 9, 10) // handle
  return flat(g)
}
function doorOpenTile() {
  const g = blank()
  // rebuild floorTile into a 2D grid then draw the door posts
  const floor = floorTile()
  for (let i = 0; i < 256; i++) g[Math.floor(i / 16)][i % 16] = floor[i]
  rectfill(g, 0, 0, 1, 15, 24)        // left post
  rectfill(g, 14, 0, 15, 15, 24)      // right post
  return flat(g)
}
function exitTile() {
  const g = blank()
  rectfill(g, 0, 0, 15, 15, 3)        // dark green
  for (let y = 0; y < 16; y++)
    for (let x = 0; x < 16; x++)
      if ((x + y) % 2 === 0) g[y][x] = 11 // bright green checker
  // up-chevron "exit"
  rectfill(g, 7, 4, 8, 12, 7)
  pixel(g, 6, 6, 7); pixel(g, 9, 6, 7); pixel(g, 5, 8, 7); pixel(g, 10, 8, 7)
  return flat(g)
}

module.exports = {
  screenW: 320, screenH: 200, scale: 4,
  cellW: 16, cellH: 16, mapW: 30, mapH: 18,
  sprites: {
    0: makeGuy(),
    1: floorTile(),
    2: wallTile(),
    3: doorTile(),
    4: doorOpenTile(),
    5: exitTile(),
  },
  // the map is built procedurally in hotline.c (build_floor) via mset(); no layout here.
}
