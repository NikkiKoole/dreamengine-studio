// config for druglord.c — pedestrians + 6 district tile-map backdrops.
//
// Slot layout (8-col sheet):
//   0,1   pedestrian type A — two walk frames (shirt = magic index 28)
//   2,3   pedestrian type B — two walk frames
//   16    street      17 grass       18 water
//   19    building(L)  20 building(H, with roof)
//   21    glass tower  22 warehouse   23 house
//   24    tower block  25 market stall 26 tree
//   27    crate        28 picket fence 29 park lamp
//
// All window pixels are drawn in palette index 10 so the cart can pal(10, …) them
// dark by day and neon-lit at night. Sprites are generated programmatically so we
// can reach extended palette indices (16–31) directly.

const { blank, pixel, rectfill, flat, noise } = require('../sprite-draw.js')

const T = 0;

// ── ground / nature ──────────────────────────────────────────────
function street() {
  const g = blank(); rectfill(g, 0, 0, 15, 15, 5);
  for (let y = 0; y < 16; y++) for (let x = 0; x < 16; x++) {
    const n = noise(x, y, 19);
    if (n === 0) g[y][x] = 6; else if (n === 4) g[y][x] = 22;
  }
  return flat(g);
}
function grass() {
  const g = blank(); rectfill(g, 0, 0, 15, 15, 3);
  for (let y = 0; y < 16; y++) for (let x = 0; x < 16; x++) {
    const n = noise(x, y, 13);
    if (n === 0) g[y][x] = 11; else if (n === 6) g[y][x] = 27;
  }
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

// ── buildings (transparent above the silhouette so sky shows through) ──
function bldg(roof) {
  const g = blank(); rectfill(g, 0, 0, 15, 15, 22);
  rectfill(g, 0, 0, 0, 15, 5); rectfill(g, 15, 0, 15, 15, 5);
  for (let y = 2; y < 15; y += 3) for (let x = 2; x < 14; x += 3) rectfill(g, x, y, x + 1, y + 1, 10);
  if (roof) rectfill(g, 0, 0, 15, 1, 16);
  return flat(g);
}
function tower() {
  const g = blank(); rectfill(g, 1, 0, 14, 15, 28);
  rectfill(g, 1, 0, 1, 15, 12);                                   // edge highlight
  for (let y = 1; y < 16; y += 3) for (let x = 3; x < 14; x += 3) rectfill(g, x, y, x + 1, y + 1, 10);
  return flat(g);
}
function warehouse() {
  const g = blank(); rectfill(g, 0, 3, 15, 15, 6); rectfill(g, 0, 0, 15, 2, 5);
  for (let x = 0; x < 16; x += 2) rectfill(g, x, 4, x, 15, 22);   // corrugation
  rectfill(g, 5, 8, 10, 15, 16);                                  // big door
  rectfill(g, 2, 5, 4, 7, 10); rectfill(g, 11, 5, 13, 7, 10);          // windows
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
  for (let x = 0; x < 16; x++) if ((x * 3) % 7 === 0) pixel(g, x, 15, 16); // grime
  return flat(g);
}
function stall() {
  const g = blank(); rectfill(g, 0, 3, 15, 15, 4);
  for (let x = 0; x < 16; x++) rectfill(g, x, 0, x, 2, x % 2 ? 8 : 7); // striped awning
  rectfill(g, 1, 4, 14, 5, 9);                                          // goods
  rectfill(g, 2, 6, 13, 9, 2);                                          // dark interior
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
  for (let i = 0; i < 11; i++) { pixel(g, 3 + i, 5 + i, 9); pixel(g, 13 - i, 5 + i, 9); } // X straps
  return flat(g);
}
function fence() {
  const g = blank(); rectfill(g, 0, 9, 15, 10, 6);
  for (let x = 1; x < 16; x += 3) { rectfill(g, x, 6, x + 1, 15, 6); pixel(g, x, 5, 6); }
  return flat(g);
}
function lamp() {
  const g = blank(); rectfill(g, 0, 0, 15, 15, 3);                      // grass base
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

// ── district backdrops: 6 strips, 4 rows × 20 cols, stacked vertically ──
const STRIPS = [
  // 0 Downtown — glass towers
  "...T......T.....T...",
  ".T.T..T...T..T..T.T.",
  "TTTTT.TTTTT.TTT.TTTT",
  "ssssssssssssssssssss",
  // 1 The Docks — warehouses, crates, water
  "....................",
  "..AAA...AAAA...AAA..",
  "AAAAAA.AAAAAA.AAAAAA",
  "wwsccsssscccssssccww",
  // 2 Suburbs — houses, trees, lawns
  "....................",
  "..r....h...r...h...r",
  ".h.r.hh.r.h.rh.r.h.r",
  "gggggggggggggggggggg",
  // 3 The Blocks — tower blocks
  "b..b..b..b..b..b..b.",
  "bb.bb.bb.bb.bb.bb.bb",
  "bbb.bbb.bbb.bbb.bbbb",
  "ssssssssssssssssssss",
  // 4 Chinatown — stalls + buildings
  "....................",
  "..LL...HHH...LL.HH..",
  "LLLLL.HHHHH.LLLL.HHH",
  "smmsmsmmsmsmmsmsmmss",
  // 5 Uptown Park — trees, lamps, lawns
  "....................",
  "..r...r...r...r...r.",
  "r.rr.r.rr.r.rr.r.rr.",
  "ggggpgggggpgggggpggg",
];
const layout = STRIPS.map(r => (r + "....................").slice(0, 20));

module.exports = {
  screenW: 320, screenH: 200, scale: 4, cellW: 16, cellH: 16,
  sprites: {
    0: ped(15, 4, false),  1: ped(15, 4, true),
    2: ped(31, 16, false), 3: ped(31, 16, true),
    16: street(), 17: grass(), 18: water(),
    19: bldg(false), 20: bldg(true),
    21: tower(), 22: warehouse(), 23: house(),
    24: block(), 25: stall(), 26: tree(),
    27: crate(), 28: fence(), 29: lamp(),
  },
  map: {
    layout,
    tiles: {
      s: 16, g: 17, w: 18, L: 19, H: 20, T: 21,
      A: 22, h: 23, b: 24, m: 25, r: 26, c: 27, f: 28, p: 29,
    },
  },
};
