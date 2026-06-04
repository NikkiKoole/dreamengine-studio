// config for masseffect.c — sprites are generated programmatically so we can use
// the extended palette + the "magic" recolor indices (28 armor / 29 accent) that
// the cart swaps per fighter with pal(), the crowd-cart trick. The cart builds
// both tile scenes (bridge + mission) at runtime with mset(), so no map here.
//
// Slot layout (8-col sheet):
//   0  — top-down soldier (hero / squad / enemy base), faces +x, rotated by spr_rot
//   1  — top-down husk enemy (also scaled 2x for the boss via sspr_ex)
//   16 — bridge floor   17 — bridge wall   18 — console   19 — galaxy table
//   20 — mission floor  21 — mission wall  22 — cover crate

const { blank, pixel, rectfill, circlefill, outlined, flat, OUT } = require('../sprite-draw.js')

const TRANS = 0
const ARMOR = 28   // MAGIC: recolored by pal(28, ...)
const ACC   = 29   // MAGIC: recolored by pal(29, ...)
const GUN   = 6    // light grey barrel
const DARK  = 5    // dark grey

// ── top-down soldier, facing right (+x) ───────────────────────────────────────
function soldier() {
  const g = blank()
  circlefill(g, 7, 8, 5.2, ARMOR)          // armoured body
  // visor / front accent toward +x
  for (let y = 6; y <= 10; y++)
    for (let x = 8; x <= 11; x++)
      if (Math.hypot(x - 7, y - 8) <= 5.2) g[y][x] = ACC
  rectfill(g, 2, 6, 3, 10, ARMOR)         // back pack
  rectfill(g, 10, 7, 15, 8, GUN)          // gun barrel forward
  pixel(g, 15, 7, DARK); pixel(g, 15, 8, DARK)
  pixel(g, 9, 7, 7); pixel(g, 9, 9, 7)   // eye glints (white)
  return flat(outlined(g))
}

// ── top-down husk enemy, facing right ─────────────────────────────────────────
function husk() {
  const g = blank()
  circlefill(g, 7, 8, 4.6, ARMOR)
  rectfill(g, 9, 7, 10, 8, ACC)           // glowing eye/mouth
  // two front claws
  pixel(g, 11, 4, GUN); pixel(g, 12, 3, GUN); pixel(g, 13, 3, GUN)
  pixel(g, 11, 12, GUN); pixel(g, 12, 13, GUN); pixel(g, 13, 13, GUN)
  pixel(g, 2, 6, ARMOR); pixel(g, 2, 10, ARMOR)  // back spikes
  return flat(outlined(g))
}

// ── tiles (fully opaque — never index 0) ──────────────────────────────────────
function bridgeFloor() {
  const g = blank(); rectfill(g, 0, 0, 15, 15, 17)   // darker blue deck
  for (let i = 0; i < 16; i++) { g[0][i] = 1; g[i][0] = 1 } // panel seams
  g[7][7] = 5; g[8][8] = 5; g[4][11] = 1; g[11][4] = 1
  return flat(g)
}
function bridgeWall() {
  const g = blank(); rectfill(g, 0, 0, 15, 15, 5)
  rectfill(g, 0, 0, 15, 2, 6); rectfill(g, 0, 8, 15, 8, 21)
  return flat(g)
}
function console_() {
  const g = blank(); rectfill(g, 0, 0, 15, 15, 16)
  rectfill(g, 1, 1, 14, 9, 5); rectfill(g, 2, 2, 13, 8, 1)  // screen
  g[4][4] = 12; g[6][7] = 11; g[3][10] = 10; g[7][5] = 12
  rectfill(g, 0, 11, 15, 15, 5)
  return flat(g)
}
function table() {
  const g = blank(); rectfill(g, 0, 0, 15, 15, 5)
  rectfill(g, 2, 2, 13, 13, 17)         // recessed top
  rectfill(g, 4, 4, 11, 9, 28)          // blue glow
  g[5][6] = 12; g[7][9] = 12; g[6][7] = 7
  return flat(g)
}
// per-planet floor/wall (map() ignores pal(), so each planet gets its own slot)
function missionFloor(base, s1, s2) {
  const g = blank(); rectfill(g, 0, 0, 15, 15, base)
  for (let y = 0; y < 16; y++)
    for (let x = 0; x < 16; x++) {
      const n = (x * 7 + y * 13) % 11
      if (n === 0) g[y][x] = s1
      else if (n === 4) g[y][x] = s2
    }
  return flat(g)
}
function missionWall(body, shade, hi) {
  const g = blank(); rectfill(g, 0, 0, 15, 15, body)
  rectfill(g, 0, 13, 15, 15, shade); rectfill(g, 0, 0, 15, 0, hi)
  rectfill(g, 0, 7, 15, 7, shade); g[3][4] = shade; g[10][11] = shade
  return flat(g)
}
function cover() {
  const g = blank()
  rectfill(g, 1, 1, 14, 14, 4)          // brown crate
  rectfill(g, 1, 1, 14, 2, 9)           // orange top trim
  rectfill(g, 1, 1, 1, 14, 20); rectfill(g, 14, 1, 14, 14, 20); rectfill(g, 1, 14, 14, 14, 20)
  rectfill(g, 7, 1, 8, 14, 20); rectfill(g, 1, 7, 14, 8, 9)
  return flat(g)
}

module.exports = {
  screenW: 320, screenH: 200, scale: 4,
  cellW: 16, cellH: 16,
  sprites: {
    0: soldier(),
    1: husk(),
    16: bridgeFloor(),
    17: bridgeWall(),
    18: console_(),
    19: table(),
    // planet 0 — Feris III (red desert)
    20: missionFloor(20, 4, 16),
    21: missionWall(4, 20, 9),
    22: cover(),
    // planet 1 — Noveria (ice)
    23: missionFloor(1, 17, 28),
    25: missionWall(28, 1, 12),
    // planet 2 — Eden Prime (garden)
    24: missionFloor(19, 3, 16),
    26: missionWall(27, 3, 11),
  },
}
