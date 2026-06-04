// config for tradewinds.c — a sloop (2 sail frames) + a sea chart tilemap.
//
// Slot layout (8-col sheet):
//   0,1   your sloop — two sail-billow frames. side view, faces right.
//         sail = index 7 (white), hull = 4 (brown), mast = 20 (dark brown),
//         flag = 14 (pink, a "magic" colour). The cart pal()-recolours these
//         for the pirate (grey sail / black hull / red flag).
//   16    deep sea     18 sand     19 palm islet     20 port town
//
// Town windows are drawn in palette index 10 so the harbour scene can
// pal(10, accent) them to glow in each port's colour. Sprites are generated
// programmatically so we can reach the extended palette indices (16–31).

const { blank, pixel, rectfill, flat, noise } = require('../sprite-draw.js')

const T = 0;

// ── the sloop ────────────────────────────────────────────────────
function ship(billow) {
  const g = blank();
  // hull
  rectfill(g, 3, 12, 12, 13, 4);
  pixel(g, 2, 12, 4); pixel(g, 13, 12, 4);
  rectfill(g, 4, 14, 11, 14, 20);     // waterline shade
  rectfill(g, 4, 11, 11, 11, 31);     // deck (peach)
  // mast
  rectfill(g, 8, 2, 8, 11, 20);
  // pennant flag (magic colour)
  rectfill(g, 9, 2, 10, 2, 14);
  // mainsail to the right of the mast, widening downward (billow toggles size)
  const ext = billow ? 6 : 5;
  for (let y = 3; y <= 10; y++) {
    let w = Math.round((y - 2) / 8 * ext) + 1;
    for (let x = 9; x < 9 + w && x < 15; x++) g[y][x] = 7;
  }
  // small jib to the left
  for (let y = 7; y <= 10; y++) {
    let w = y - 6;
    for (let x = 8 - w; x < 8 && x >= 2; x++) g[y][x] = 7;
  }
  return flat(g);
}

// ── sea chart tiles ──────────────────────────────────────────────
function sea() {
  const g = blank(); rectfill(g, 0, 0, 15, 15, 17);              // darker blue base
  for (let y = 0; y < 16; y++) for (let x = 0; x < 16; x++) {
    const n = noise(x, y, 13);
    if (n === 0) g[y][x] = 1; else if (n === 6) g[y][x] = 28;
  }
  return flat(g);
}
function sand() {
  const g = blank(); rectfill(g, 0, 0, 15, 15, 15);              // light peach
  for (let y = 0; y < 16; y++) for (let x = 0; x < 16; x++) {
    const n = noise(x, y, 11);
    if (n === 0) g[y][x] = 31; else if (n === 5) g[y][x] = 4;
  }
  // a touch of foam on the edges
  for (let x = 0; x < 16; x++) { if ((x * 3) % 5 === 0) { pixel(g, x, 0, 7); pixel(g, x, 15, 7); } }
  return flat(g);
}
function palm() {
  const g = blank(); rectfill(g, 0, 12, 15, 15, 15);             // sandy base
  rectfill(g, 7, 5, 8, 12, 20);                                  // trunk
  rectfill(g, 4, 3, 11, 5, 3);                                   // fronds
  rectfill(g, 3, 4, 3, 5, 3); rectfill(g, 12, 4, 12, 5, 3);
  pixel(g, 7, 2, 11); pixel(g, 8, 2, 11);
  for (let x = 4; x < 12; x++) if ((x * 5) % 3 === 0) pixel(g, x, 4, 11);
  return flat(g);
}
function town() {
  const g = blank(); rectfill(g, 0, 12, 15, 15, 15);             // sand
  // house A (grey wall, red roof)
  rectfill(g, 2, 6, 6, 12, 6); rectfill(g, 2, 5, 6, 5, 8);
  rectfill(g, 3, 8, 4, 9, 10);                                   // window (magic glow index 10)
  // house B (taller)
  rectfill(g, 8, 4, 13, 12, 22); rectfill(g, 8, 3, 13, 3, 8);
  rectfill(g, 9, 6, 10, 7, 10); rectfill(g, 11, 9, 12, 10, 10);
  // little dock
  rectfill(g, 0, 13, 2, 13, 20);
  return flat(g);
}

// ── sea chart layout: 20 × 13, sea everywhere, a plus-shaped isle per port ──
const COLS = 20, ROWS = 13;
const PORTS = [[2, 1], [17, 1], [16, 5], [3, 6], [9, 8], [18, 8]];  // MUST match NX/NY cells in tradewinds.c
const grid = Array.from({ length: ROWS }, () => new Array(COLS).fill('~'));
function setc(x, y, c) { if (x >= 0 && x < COLS && y >= 0 && y < ROWS) grid[y][x] = c; }
for (const [cx, cy] of PORTS) {
  setc(cx, cy + 1, 's'); setc(cx - 1, cy, 's'); setc(cx + 1, cy, 's');
  setc(cx, cy - 1, 'm');   // a palm above
  setc(cx, cy, 'P');       // the town in the middle
}
const layout = grid.map(r => r.join(''));

module.exports = {
  screenW: 320, screenH: 200, scale: 4, cellW: 16, cellH: 16,
  sprites: {
    0: ship(false), 1: ship(true),
    16: sea(), 18: sand(), 19: palm(), 20: town(),
  },
  map: {
    layout,
    tiles: { '~': 16, 's': 18, 'm': 19, 'P': 20 },
  },
};
