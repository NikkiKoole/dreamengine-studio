// SKY STRIKE — sprites + scrolling sea/island/airbase map.
// Sprites are generated as flat 16×16 index arrays (reaches the magic body index 28
// used for pal()-recolored enemy squadrons, like crowd.cart.js). Craft are drawn on
// the left half and mirrored so they stay symmetric.

const { blank, pixel, rectfill, mirror, flat, OUT } = require('../sprite-draw.js')

const M = 28   // magic body index — swapped per enemy with pal()
const D = 16   // dark outline / plating (CLR_BROWNISH_BLACK)

// player jet (slot 0) — points UP, fixed grey/blue colors
function player() {
  const g = blank()
  rectfill(g, 6, 1, 7, 13, 6)      // fuselage
  rectfill(g, 7, 0, 7, 1, 7)       // nose tip
  rectfill(g, 6, 2, 6, 9, 7)       // highlight stripe
  rectfill(g, 7, 3, 7, 5, 12)      // cockpit
  rectfill(g, 1, 7, 7, 8, 6)       // main wing
  rectfill(g, 0, 7, 1, 8, 5)       // wingtip
  rectfill(g, 4, 11, 7, 12, 6)     // tail wing
  pixel(g, 4, 11, 5)
  rectfill(g, 6, 13, 7, 13, 5)     // engine
  rectfill(g, 6, 14, 7, 14, 9)     // flame
  pixel(g, 7, 15, 10)
  mirror(g); return flat(g)
}

// popcorn flyer (slot 3) — small, points DOWN, magic body
function popcorn() {
  const g = blank()
  rectfill(g, 6, 3, 7, 12, M)
  rectfill(g, 7, 12, 7, 13, M)     // nose
  rectfill(g, 7, 5, 7, 7, 12)      // cockpit
  rectfill(g, 3, 5, 7, 7, M)       // wing
  rectfill(g, 2, 6, 3, 7, D)       // wingtip
  rectfill(g, 6, 2, 7, 3, D)       // tail
  mirror(g); return flat(g)
}

// popper (slot 4) — fires aimed shots, gun nubs
function popper() {
  const g = blank()
  rectfill(g, 5, 3, 7, 12, M)
  rectfill(g, 6, 12, 7, 14, M)
  rectfill(g, 7, 5, 7, 7, 12)
  rectfill(g, 1, 7, 7, 9, M)       // wing
  rectfill(g, 2, 9, 3, 12, 8)      // gun nub (red)
  rectfill(g, 6, 2, 7, 3, D)
  mirror(g); return flat(g)
}

// gunship (slot 5) — heavy, spread fire
function gunship() {
  const g = blank()
  rectfill(g, 4, 2, 7, 13, M)
  rectfill(g, 0, 6, 7, 10, M)      // wide wing
  rectfill(g, 6, 4, 7, 7, 12)      // cockpit
  rectfill(g, 2, 8, 3, 10, 8)      // weapon cores
  rectfill(g, 6, 13, 7, 15, M)     // nose
  rectfill(g, 5, 1, 7, 2, D)
  rectfill(g, 0, 6, 1, 10, D)      // wing edge
  mirror(g); return flat(g)
}

// ground turret (slot 6) — rides the scroll, barrel points down
function turret() {
  const g = blank()
  rectfill(g, 3, 4, 7, 12, 5)      // base
  rectfill(g, 4, 5, 7, 11, D)      // ring
  rectfill(g, 5, 6, 7, 10, M)      // dome (recolored)
  rectfill(g, 7, 7, 7, 9, 8)       // core
  rectfill(g, 6, 11, 7, 15, 5)     // barrel
  mirror(g); return flat(g)
}

// ---- 32×32 boss across slots 8,9,10,11 -----------------------------------
function bossSheet() {
  const g = blank(32, 32)
  rectfill(g, 4, 2, 15, 24, M)       // hull
  rectfill(g, 0, 8, 15, 15, M)       // wing
  rectfill(g, 0, 8, 1, 15, D)        // wingtip plating
  rectfill(g, 0, 11, 15, 11, D)      // plating lines
  rectfill(g, 4, 18, 15, 18, D)
  rectfill(g, 11, 4, 15, 9, 12)      // bridge / cockpit
  rectfill(g, 7, 13, 10, 17, 8)      // side weak core
  rectfill(g, 13, 19, 15, 23, 8)     // center core
  rectfill(g, 5, 24, 8, 29, 5)       // side cannon
  rectfill(g, 13, 24, 15, 30, 5)     // center cannon
  rectfill(g, 12, 0, 15, 1, 9)       // engine glow
  mirror(g)
  // split into four 16×16 slots
  const slot = (cx, cy) => flat(g.slice(cy, cy + 16).map(row => row.slice(cx, cx + 16)))
  return { 8: slot(0, 0), 9: slot(16, 0), 10: slot(0, 16), 11: slot(16, 16) }
}

// ---- map tiles ------------------------------------------------------------
function oceanA() {
  const g = blank(); rectfill(g, 0, 0, 15, 15, 1)         // deep blue base (never index 0 → no transparency)
  rectfill(g, 2, 3, 5, 3, 12); rectfill(g, 9, 7, 12, 7, 12); rectfill(g, 4, 11, 7, 11, 12); rectfill(g, 11, 13, 14, 13, 12)
  rectfill(g, 1, 8, 3, 8, 17); rectfill(g, 8, 2, 10, 2, 17)
  return flat(g)
}
function oceanB() {
  const g = blank(); rectfill(g, 0, 0, 15, 15, 1)
  rectfill(g, 2, 3, 5, 3, 12); rectfill(g, 9, 7, 12, 7, 12); rectfill(g, 4, 11, 7, 11, 12); rectfill(g, 11, 13, 14, 13, 12)
  rectfill(g, 1, 8, 3, 8, 17); rectfill(g, 8, 2, 10, 2, 17)
  rectfill(g, 5, 5, 8, 5, 6); rectfill(g, 6, 5, 7, 5, 7); rectfill(g, 2, 12, 4, 12, 6)   // whitecaps
  return flat(g)
}
function sand() {
  const g = blank(); rectfill(g, 0, 0, 15, 15, 15)
  rectfill(g, 2, 2, 3, 3, 4); rectfill(g, 9, 5, 10, 6, 4); rectfill(g, 5, 11, 6, 12, 4); rectfill(g, 12, 10, 13, 11, 4); rectfill(g, 7, 8, 8, 8, 9)
  return flat(g)
}
function grass() {
  const g = blank(); rectfill(g, 0, 0, 15, 15, 3)
  rectfill(g, 2, 3, 3, 3, 11); rectfill(g, 8, 6, 9, 6, 11); rectfill(g, 5, 11, 6, 11, 11); rectfill(g, 11, 9, 12, 9, 4); rectfill(g, 13, 2, 13, 2, 11)
  return flat(g)
}
function runway() {
  const g = blank(); rectfill(g, 0, 0, 15, 15, 5)
  rectfill(g, 0, 0, 1, 15, 6); rectfill(g, 14, 0, 15, 15, 6)                       // edges
  rectfill(g, 7, 1, 8, 4, 10); rectfill(g, 7, 7, 8, 10, 10); rectfill(g, 7, 13, 8, 15, 10) // dashed center stripe
  return flat(g)
}

// ---- build the scrolling map (16 wide × 40 tall) --------------------------
const MW = 16, MH = 40
function buildLayout() {
  const grid = []
  for (let y = 0; y < MH; y++) grid.push(new Array(MW).fill('~'))
  const caps = [[3, 4], [11, 6], [14, 9], [6, 15], [13, 18], [2, 25], [9, 33], [5, 30]]
  for (const [x, y] of caps) grid[y][x] = 'w'
  const island = (cx, cy, rg, rs) => {
    for (let y = 0; y < MH; y++) for (let x = 0; x < MW; x++) {
      const d = Math.hypot(x - cx, y - cy)
      if (d <= rs) grid[y][x] = 's'
      if (d <= rg) grid[y][x] = 'g'
    }
  }
  island(4, 11, 1.7, 3.0)
  island(11, 24, 1.9, 3.2)
  for (let y = 29; y <= 35; y++) for (let x = 5; x <= 10; x++) grid[y][x] = 's'   // airbase apron
  for (let y = 30; y <= 34; y++) for (let x = 6; x <= 9; x++) grid[y][x] = 'r'    // runway
  return grid.map(r => r.join(''))
}

module.exports = {
  screenW: 240, screenH: 320, scale: 3,
  cellW: 16, cellH: 16, mapW: MW, mapH: MH,
  sprites: Object.assign(
    { 0: player(), 3: popcorn(), 4: popper(), 5: gunship(), 6: turret(),
      40: oceanA(), 41: oceanB(), 42: sand(), 43: grass(), 44: runway() },
    bossSheet()
  ),
  map: { layout: buildLayout(), tiles: { '~': 40, 'w': 41, 's': 42, 'g': 43, 'r': 44 } },
}
