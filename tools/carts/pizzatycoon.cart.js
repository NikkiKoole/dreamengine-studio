// config for pizzatycoon.c — pedestrians + 6 district scene strips.
//
// Slot layout (8-col sheet):
//   0,1   pedestrian type A — two walk frames (shirt = magic index 28)
//   2,3   pedestrian type B — two walk frames
//   16 street      17 pavement   18 water
//   19 townhouse   20 PIZZERIA   21 glass tower
//   22 warehouse   23 house      24 tower block
//   25 market stall 26 tree      27 crate      28 park lamp
//
// All lit window pixels are palette index 10 so the cart can pal(10, …) them
// dark by day, neon by night. Sprites are generated in JS so we can reach the
// extended palette indices (16–31) directly.

const { blank, pixel, rectfill, flat, noise } = require('../sprite-draw.js')

const T = 0;

// ── ground ───────────────────────────────────────────────────────
function street() {
  const g = blank(); rectfill(g, 0, 0, 15, 15, 5);
  for (let y = 0; y < 16; y++) for (let x = 0; x < 16; x++) {
    const n = noise(x, y, 19);
    if (n === 0) g[y][x] = 6; else if (n === 4) g[y][x] = 22;
  }
  return flat(g);
}
function pave() {
  const g = blank(); rectfill(g, 0, 0, 15, 15, 6);
  for (let x = 0; x < 16; x++) { pixel(g, x, 0, 5); pixel(g, 0, x, 5); }   // slab grid
  for (let y = 4; y < 16; y += 5) rectfill(g, 0, y, 15, y, 5);
  for (let x = 5; x < 16; x += 5) rectfill(g, x, 0, x, 15, 5);
  return flat(g);
}
function water() {
  const g = blank(); rectfill(g, 0, 0, 15, 15, 28);
  for (let y = 0; y < 16; y++) for (let x = 0; x < 16; x++) {
    const n = noise(x, y, 11);
    if (n === 0) g[y][x] = 12; else if (n === 6) g[y][x] = 1;
  }
  return flat(g);
}

// ── buildings (transparent above the silhouette so the sky shows) ──
function townhouse() {
  const g = blank(); rectfill(g, 0, 2, 15, 15, 24);                // warm brick
  rectfill(g, 0, 0, 15, 1, 16);                                    // roof cap
  rectfill(g, 0, 2, 0, 15, 20); rectfill(g, 15, 2, 15, 15, 20);
  for (let y = 3; y < 15; y += 4) for (let x = 2; x < 14; x += 4) rectfill(g, x, y, x + 1, y + 2, 10);
  return flat(g);
}
function pizzeria() {
  const g = blank(); rectfill(g, 0, 4, 15, 15, 4);                 // wall
  for (let x = 0; x < 16; x++) rectfill(g, x, 0, x, 3, x % 2 ? 8 : 7); // red/white awning
  rectfill(g, 1, 5, 14, 6, 23);                                    // sign band
  pixel(g, 4, 5, 8); pixel(g, 7, 5, 8); pixel(g, 10, 5, 8);         // sign lettering hint
  rectfill(g, 5, 8, 10, 15, 10);                                   // glowing window/door (magic 10)
  rectfill(g, 2, 8, 4, 11, 10); rectfill(g, 11, 8, 13, 11, 10);         // side windows
  return flat(g);
}
function tower() {
  const g = blank(); rectfill(g, 1, 0, 14, 15, 28);
  rectfill(g, 1, 0, 1, 15, 12);                                    // edge highlight
  for (let y = 1; y < 16; y += 3) for (let x = 3; x < 14; x += 3) rectfill(g, x, y, x + 1, y + 1, 10);
  return flat(g);
}
function warehouse() {
  const g = blank(); rectfill(g, 0, 3, 15, 15, 6); rectfill(g, 0, 0, 15, 2, 5);
  for (let x = 0; x < 16; x += 2) rectfill(g, x, 4, x, 15, 22);    // corrugation
  rectfill(g, 5, 8, 10, 15, 16);                                   // big door
  rectfill(g, 2, 5, 4, 7, 10); rectfill(g, 11, 5, 13, 7, 10);           // windows
  return flat(g);
}
function house() {
  const g = blank();
  for (let y = 0; y < 5; y++) rectfill(g, 6 - y, y, 9 + y, y, 24); // pitched roof
  rectfill(g, 3, 5, 12, 15, 15);                                   // walls
  rectfill(g, 7, 9, 9, 15, 4);                                     // door
  rectfill(g, 4, 7, 5, 9, 10); rectfill(g, 10, 7, 11, 9, 10);           // windows
  return flat(g);
}
function block() {
  const g = blank(); rectfill(g, 0, 0, 15, 15, 5); rectfill(g, 0, 0, 0, 15, 22);
  for (let y = 1; y < 15; y += 3) for (let x = 2; x < 15; x += 3) rectfill(g, x, y, x + 1, y + 1, 10);
  for (let x = 0; x < 16; x++) if ((x * 3) % 7 === 0) pixel(g, x, 15, 16);
  return flat(g);
}
function stall() {
  const g = blank(); rectfill(g, 0, 3, 15, 15, 4);
  for (let x = 0; x < 16; x++) rectfill(g, x, 0, x, 2, x % 2 ? 8 : 7); // striped awning
  rectfill(g, 1, 4, 14, 5, 9);                                          // goods
  rectfill(g, 2, 6, 13, 9, 2);                                          // interior
  rectfill(g, 1, 10, 2, 15, 20); rectfill(g, 13, 10, 14, 15, 20);            // posts
  return flat(g);
}
function tree() {
  const g = blank(); rectfill(g, 7, 10, 8, 15, 4);                      // trunk
  rectfill(g, 3, 2, 12, 9, 3); rectfill(g, 4, 1, 11, 1, 3);
  rectfill(g, 2, 4, 2, 7, 3); rectfill(g, 13, 4, 13, 7, 3);
  for (let y = 2; y < 9; y++) for (let x = 3; x < 13; x++) {
    if (noise(x, y, 6) === 0) g[y][x] = 11; else if ((x + y) % 5 === 0) g[y][x] = 27;
  }
  return flat(g);
}
function crate() {
  const g = blank(); rectfill(g, 2, 4, 13, 15, 4);
  rectfill(g, 2, 4, 13, 4, 20); rectfill(g, 2, 15, 13, 15, 20);
  rectfill(g, 2, 4, 2, 15, 20); rectfill(g, 13, 4, 13, 15, 20);
  for (let i = 0; i < 11; i++) { pixel(g, 3 + i, 5 + i, 9); pixel(g, 13 - i, 5 + i, 9); }
  return flat(g);
}
function lamp() {
  const g = blank(); rectfill(g, 0, 0, 15, 15, 6);                      // pavement base
  rectfill(g, 7, 4, 8, 15, 5);                                          // pole
  rectfill(g, 6, 2, 9, 4, 10);                                          // globe (glows at night)
  return flat(g);
}

// ── pedestrian (16×16, shirt in magic index 28) ──────────────────
function ped(skin, hair, raise) {
  const g = blank();
  rectfill(g, 5, 1, 10, 2, hair); rectfill(g, 5, 3, 10, 6, skin);            // head
  pixel(g, 6, 5, 16); pixel(g, 9, 5, 16);                              // eyes
  rectfill(g, 5, 7, 10, 11, 28); rectfill(g, 4, 7, 4, 10, 28); rectfill(g, 11, 7, 11, 10, 28); // shirt + arms
  if (raise) { rectfill(g, 5, 12, 6, 14, 1); rectfill(g, 9, 12, 10, 15, 1); }
  else       { rectfill(g, 5, 12, 6, 15, 1); rectfill(g, 9, 12, 10, 14, 1); }
  rectfill(g, 5, 15, 6, 15, 16); rectfill(g, 9, 15, 10, 15, 16);             // shoes
  return flat(g);
}

// ── district scene strips: 6 × (4 rows × 20 cols), stacked vertically ──
const STRIPS = [
  // 0 Little Italy — townhouses + a pizzeria
  "....................",
  "..tt..tt..Z..tt..tt.",
  ".tttt.tttt.tttt.tttt",
  "ssssssssssssssssssss",
  // 1 The Docks — warehouses, crates, water frontage
  "....................",
  "..AAA...Z....AAA....",
  "AAAAAA.AAAAAA.AAAAAA",
  "wwccssssccsssscccsww",
  // 2 Uptown — glass towers
  "...T......T.....T...",
  ".T.T..T..ZT..T..T.T.",
  "TTTTT.TTTTT.TTT.TTTT",
  "pppppppppppppppppppp",
  // 3 Campus — big blocks
  "b..b..b..b..b..b..b.",
  "bb.bb.Z..bb.bb.bb.bb",
  "bbb.bbb.bbb.bbb.bbbb",
  "pppppppppppppppppppp",
  // 4 Suburbs — houses, trees, lawns
  "....................",
  "..h....r...Z...h...r",
  ".h.r.hh.r.h.rh.r.h.r",
  "ssssssssssssssssssss",
  // 5 The Piazza — stalls, townhouses, a tree
  "....................",
  "..mm...tt...Z..mm.r.",
  "mmmmm.tttt.tttt.mmmm",
  "ppppplppppppplpppppp",
];
const layout = STRIPS.map(r => (r + "....................").slice(0, 20));

module.exports = {
  screenW: 320, screenH: 200, scale: 4, cellW: 16, cellH: 16,
  sprites: {
    0: ped(15, 4, false),  1: ped(15, 4, true),
    2: ped(31, 16, false), 3: ped(31, 16, true),
    16: street(), 17: pave(), 18: water(),
    19: townhouse(), 20: pizzeria(), 21: tower(),
    22: warehouse(), 23: house(), 24: block(),
    25: stall(), 26: tree(), 27: crate(), 28: lamp(),
  },
  map: {
    layout,
    tiles: {
      s: 16, p: 17, w: 18, t: 19, Z: 20, T: 21,
      A: 22, h: 23, b: 24, m: 25, r: 26, c: 27, l: 28,
    },
  },
};
