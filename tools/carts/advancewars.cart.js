// Sprites for advancewars.c — Advance Wars demake "FIELD COMMANDER".
// Generated programmatically to reach the extended palette and the
// "magic" team colors 28 (body) / 29 (dark) that pal() recolors per army.
//
// Slot layout (8-col sheet):
//   1 plains  2 forest  3 mountain  4 road  5 sea  6 shoal     (terrain, opaque)
//   7 city    8 factory 9 hq                                   (buildings, magic 28/29)
//  16 infantry 17 mech 18 recon 19 tank 20 artillery           (units, magic 28/29)

const { blank, pixel, rectfill, outlined, flat, noise, OUT } = require('../sprite-draw.js')

// ── terrain (fully opaque — no index 0, so nothing turns transparent) ──────────
function plains() {
  const g = blank(); rectfill(g, 0, 0, 15, 15, 27);
  for (let y = 0; y < 16; y++) for (let x = 0; x < 16; x++) {
    const n = noise(x, y, 19);
    if (n === 0) g[y][x] = 11; else if (n === 6) g[y][x] = 3;
  }
  return flat(g);
}
function forest() {
  const g = blank(); rectfill(g, 0, 0, 15, 15, 3);
  const trees = [[3, 4], [10, 3], [7, 9]];
  for (const [tx, ty] of trees) {
    rectfill(g, tx - 1, ty, tx + 2, ty + 2, 27);
    rectfill(g, tx, ty - 1, tx + 1, ty - 1, 11);
    pixel(g, tx, ty + 3, 4); pixel(g, tx + 1, ty + 3, 4);
  }
  return flat(g);
}
function mountain() {
  const g = blank(); rectfill(g, 0, 0, 15, 15, 3);
  for (let y = 3; y < 16; y++) { const w = Math.min(7, y - 1); rectfill(g, 8 - w, y, 7 + w, y, 22); }
  rectfill(g, 5, 4, 10, 6, 5);
  rectfill(g, 6, 3, 9, 4, 6);
  rectfill(g, 7, 3, 8, 3, 7);
  return flat(g);
}
function road() {
  const g = blank(); rectfill(g, 0, 0, 15, 15, 4);
  rectfill(g, 4, 0, 11, 15, 6);
  for (let y = 1; y < 16; y += 3) rectfill(g, 7, y, 8, y + 1, 23);
  return flat(g);
}
function sea() {
  const g = blank(); rectfill(g, 0, 0, 15, 15, 12);
  for (let y = 0; y < 16; y++) for (let x = 0; x < 16; x++) {
    const n = noise(x, y, 11);
    if (n === 0) g[y][x] = 28; else if (n === 4) g[y][x] = 1;
  }
  rectfill(g, 2, 3, 5, 3, 7); rectfill(g, 9, 9, 12, 9, 7);
  return flat(g);
}
function shoal() {
  const g = blank(); rectfill(g, 0, 0, 15, 15, 15);
  for (let y = 0; y < 16; y++) for (let x = 0; x < 16; x++)
    if (noise(x, y, 13) === 0) g[y][x] = 9;
  rectfill(g, 0, 13, 15, 15, 12);
  return flat(g);
}

// ── buildings (transparent bg, body = magic 28, dark = magic 29, white = 7) ─────
function city() {
  const g = blank();
  rectfill(g, 2, 6, 7, 14, 28); rectfill(g, 8, 8, 13, 14, 28);
  rectfill(g, 2, 4, 7, 5, 29);  rectfill(g, 8, 6, 13, 7, 29);
  rectfill(g, 4, 8, 5, 9, 7);   rectfill(g, 10, 10, 11, 11, 7);
  rectfill(g, 4, 12, 5, 14, 29);
  return flat(outlined(g));
}
function factory() {
  const g = blank();
  rectfill(g, 2, 6, 13, 14, 28);
  rectfill(g, 2, 4, 13, 5, 29);
  rectfill(g, 10, 1, 11, 5, 29);
  pixel(g, 10, 0, 6); pixel(g, 11, -1, 6);
  rectfill(g, 5, 9, 10, 14, 29);
  rectfill(g, 6, 10, 9, 13, 5);
  rectfill(g, 3, 7, 4, 8, 7);
  return flat(outlined(g));
}
function hq() {
  const g = blank();
  rectfill(g, 4, 8, 11, 14, 28);
  rectfill(g, 6, 3, 9, 8, 28);
  rectfill(g, 4, 7, 11, 7, 29);
  rectfill(g, 7, 5, 8, 6, 7);
  rectfill(g, 10, 1, 10, 6, 5);
  rectfill(g, 11, 1, 14, 3, 29);
  return flat(outlined(g));
}

// ── units (transparent; body magic 28, dark magic 29, grey 5, skin 15) ─────────
function infantry() {
  const g = blank();
  rectfill(g, 5, 3, 10, 4, 28);
  rectfill(g, 6, 5, 9, 7, 15);
  rectfill(g, 6, 8, 9, 11, 28);
  rectfill(g, 5, 8, 5, 10, 28); rectfill(g, 10, 8, 10, 10, 28);
  rectfill(g, 10, 6, 13, 7, 5);
  rectfill(g, 6, 12, 7, 14, 5); rectfill(g, 8, 12, 9, 14, 5);
  return flat(outlined(g));
}
function mech() {
  const g = blank();
  rectfill(g, 5, 3, 10, 4, 28);
  rectfill(g, 6, 5, 9, 7, 15);
  rectfill(g, 5, 8, 10, 12, 28);
  rectfill(g, 2, 6, 9, 8, 5);
  pixel(g, 2, 5, 9);
  rectfill(g, 6, 13, 7, 15, 5); rectfill(g, 8, 13, 9, 15, 5);
  return flat(outlined(g));
}
function recon() {
  const g = blank();
  rectfill(g, 2, 7, 13, 11, 28);
  rectfill(g, 5, 5, 10, 7, 12);
  rectfill(g, 2, 11, 13, 11, 29);
  rectfill(g, 3, 12, 5, 13, 5); rectfill(g, 10, 12, 12, 13, 5);
  pixel(g, 3, 12, OUT); pixel(g, 10, 12, OUT);
  pixel(g, 13, 8, 9);
  return flat(outlined(g));
}
function tank() {
  const g = blank();
  rectfill(g, 2, 11, 13, 13, 5);
  rectfill(g, 3, 8, 12, 11, 28);
  rectfill(g, 5, 5, 10, 8, 29);
  rectfill(g, 10, 6, 15, 6, 5);
  pixel(g, 7, 6, 7);
  return flat(outlined(g));
}
function arty() {
  const g = blank();
  rectfill(g, 2, 11, 13, 13, 5);
  rectfill(g, 3, 8, 11, 11, 28);
  rectfill(g, 4, 6, 7, 9, 29);
  const bar = [[7, 8], [8, 7], [9, 6], [10, 5], [11, 4], [12, 3], [13, 2], [14, 1]];
  for (const [bx, by] of bar) { pixel(g, bx, by, 5); pixel(g, bx, by + 1, 5); }
  return flat(outlined(g));
}

module.exports = {
  screenW: 320, screenH: 200, scale: 4,
  cellW: 16, cellH: 16, mapW: 128, mapH: 64,
  sprites: {
    1: plains(),  2: forest(),  3: mountain(), 4: road(), 5: sea(), 6: shoal(),
    7: city(),    8: factory(), 9: hq(),
    16: infantry(), 17: mech(), 18: recon(), 19: tank(), 20: arty(),
  },
}
