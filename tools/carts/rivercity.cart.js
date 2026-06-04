// config for rivercity.c — the whole brawler cast (hero, every thug, the boss)
// is ONE 16x32 character split across two 16x16 slots, drawn in "magic" colors
// the cart recolors per fighter with pal() (the crowd.c trick):
//
//   SHIRT = index 28   pal(28, gangShirt)
//   PANTS = index 29   pal(29, gangPants)
//   HAIR  = index 26   pal(26, gangHair)
//
// Slots:
//   0       — character TOP   (hair + face + upper torso/sleeves in SHIRT)
//   8,9,10,11 — shared LEG frames (lower torso SHIRT, hips/legs PANTS, shoes):
//               a 4-step march cycle [neutral, left-up, neutral, right-up]
//   16..23  — floor tiles, two per zone (downtown / park / mall / docks)

const { blank, pixel, rectfill, outlined, flat, OUT } = require('../sprite-draw.js')

const TRANS = 0, SKIN = 15, SHOE = 5
const SHIRT = 28, PANTS = 29, HAIR = 26   // MAGIC — recolored per fighter

// ── character TOP (figure rows 0-15) ──────────────────────────────────────
function makeTop() {
  const g = blank()
  rectfill(g, 4, 10, 11, 15, SHIRT)    // chest, flush to bottom edge → clean seam
  rectfill(g, 3, 11, 3, 15, SHIRT)     // left sleeve
  rectfill(g, 12, 11, 12, 15, SHIRT)   // right sleeve
  rectfill(g, 5, 4, 10, 9, SKIN)       // face
  rectfill(g, 4, 2, 11, 4, HAIR)       // hair cap
  rectfill(g, 5, 5, 10, 5, HAIR)       // fringe
  rectfill(g, 4, 5, 4, 7, HAIR)        // sideburns
  rectfill(g, 11, 5, 11, 7, HAIR)
  pixel(g, 6, 7, OUT); pixel(g, 9, 7, OUT)   // eyes
  return flat(outlined(g))
}

// ── shared LEG frame (figure rows 16-31): lower torso, arms, pants, shoes ──
function makeLegs(leftUp, rightUp) {
  const g = blank()
  rectfill(g, 4, 0, 11, 3, SHIRT)      // lower torso (shirt), flush to top edge
  rectfill(g, 3, 0, 3, 2, SHIRT)       // forearms
  rectfill(g, 12, 0, 12, 2, SHIRT)
  pixel(g, 3, 3, SKIN); pixel(g, 12, 3, SKIN)   // hands
  rectfill(g, 4, 4, 11, 6, PANTS)      // pelvis / hips
  for (const L of [{ x0: 5, x1: 6, up: leftUp }, { x0: 9, x1: 10, up: rightUp }]) {
    const footY = L.up ? 10 : 12
    rectfill(g, L.x0, 7, L.x1, footY, PANTS)
    rectfill(g, L.x0, footY + 1, L.x1, footY + 2, SHOE)
  }
  return flat(outlined(g))
}

// ── opaque floor tiles (avoid index 0 so nothing turns transparent) ────────
function tile(base, fleckA, fleckB) {
  const g = blank()
  rectfill(g, 0, 0, 15, 15, base)
  for (let y = 0; y < 16; y++) for (let x = 0; x < 16; x++) {
    const n = (x * 7 + y * 13) % 17
    if (n === 0 && fleckA != null) g[y][x] = fleckA
    else if (n === 5 && fleckB != null) g[y][x] = fleckB
  }
  return flat(g)
}
function checker(a, b) {
  const g = blank()
  for (let y = 0; y < 16; y++) for (let x = 0; x < 16; x++)
    g[y][x] = ((Math.floor(x / 4) + Math.floor(y / 4)) % 2) ? a : b
  return flat(g)
}
function planks(base, line) {
  const g = blank()
  rectfill(g, 0, 0, 15, 15, base)
  for (let x = 0; x < 16; x++) { g[0][x] = line; g[8][x] = line }   // plank seams
  return flat(g)
}

module.exports = {
  screenW: 320, screenH: 200, scale: 4,
  cellW: 16, cellH: 16, mapW: 128, mapH: 64,
  sprites: {
    0:  makeTop(),
    8:  makeLegs(false, false),   // neutral
    9:  makeLegs(true,  false),   // left foot up
    10: makeLegs(false, false),   // neutral
    11: makeLegs(false, true),    // right foot up
    // floor tiles, two per zone
    16: tile(6, 5, 22),           // downtown pavement
    17: tile(5, 16, 6),           // downtown manhole / dark patch
    18: tile(3, 11, 27),          // park grass
    19: tile(3, 10, 8),           // park grass + flowers
    20: checker(6, 7),            // mall tile (light)
    21: checker(13, 5),           // mall tile (dark)
    22: planks(4, 20),            // dock planks
    23: planks(20, 16),           // dock planks (worn)
  },
}
