// Sprites for heroes.c — "HEROES OF MIGHT & MAGIC" (a tiny HoMM demake).
// Flat index arrays so we can reach the extended palette and the "magic" team
// colors that pal() recolors per owner:  28 = body/banner, 29 = dark/shadow.
// Crowd-cart trick: hero, town and every creature share these magic indices and
// get tinted blue (player) / red (CPU) / grey (neutral) at draw time.
//
// Slot layout (8-col sheet):
//   1 grass  2 forest  3 mountain  4 water  5 road            (overworld terrain, opaque)
//   6 gold pile  7 ore pile  8 chest  9 mine                  (map objects, transparent)
//  10 town/castle (magic 28/29)  11 hero (magic 28/29)
//  12 battle field (opaque)  13 battle rock (obstacle)
//  16 pikeman  17 archer  18 griffin  19 ogre                 (creatures, magic 28/29)

const { blank, pixel, rectfill, outlined, flat, OUT } = require('../sprite-draw.js')

const S = 16;

// ── terrain (fully opaque — no index 0) ────────────────────────────────────────
function grass() {
  const g = blank(); rectfill(g, 0, 0, 15, 15, 27);          // medium green
  for (let y = 0; y < S; y++) for (let x = 0; x < S; x++) {
    const n = (x * 7 + y * 13) % 17;
    if (n === 0) g[y][x] = 11; else if (n === 5) g[y][x] = 3;
  }
  return flat(g);
}
function forest() {
  const g = blank(); rectfill(g, 0, 0, 15, 15, 3);           // dark-green floor
  const trees = [[3, 5], [10, 4], [7, 10]];
  for (const [tx, ty] of trees) {
    rectfill(g, tx - 1, ty, tx + 2, ty + 2, 27);             // canopy
    rectfill(g, tx, ty - 1, tx + 1, ty - 1, 11);             // highlight
    pixel(g, tx, ty + 3, 20); pixel(g, tx + 1, ty + 3, 20); // trunk
  }
  return flat(g);
}
function mountain() {
  const g = blank(); rectfill(g, 0, 0, 15, 15, 3);           // grass skirt
  for (let y = 3; y < 16; y++) { const w = Math.min(7, y - 1); rectfill(g, 8 - w, y, 7 + w, y, 22); }
  rectfill(g, 5, 5, 10, 7, 5);                               // shadow flank
  rectfill(g, 6, 3, 9, 4, 6); rectfill(g, 7, 2, 8, 2, 7);         // peak + snow
  return flat(g);
}
function water() {
  const g = blank(); rectfill(g, 0, 0, 15, 15, 28);          // (magic) deep — but we want fixed blue
  rectfill(g, 0, 0, 15, 15, 12);
  for (let y = 0; y < S; y++) for (let x = 0; x < S; x++) {
    const n = (x * 5 + y * 9) % 11;
    if (n === 0) g[y][x] = 28; else if (n === 4) g[y][x] = 1;
  }
  rectfill(g, 2, 4, 5, 4, 7); rectfill(g, 9, 10, 12, 10, 7);      // foam
  return flat(g);
}
function road() {
  const g = blank(); rectfill(g, 0, 0, 15, 15, 27);          // grass shoulders
  rectfill(g, 4, 0, 11, 15, 4);                              // brown track
  rectfill(g, 5, 0, 10, 15, 20);                             // darker rut
  for (let y = 1; y < 16; y += 4) rectfill(g, 7, y, 8, y + 1, 23); // dashes
  return flat(g);
}
function battlefield() {
  const g = blank(); rectfill(g, 0, 0, 15, 15, 20);          // packed dirt
  for (let y = 0; y < S; y++) for (let x = 0; x < S; x++) {
    const n = (x * 3 + y * 7) % 13;
    if (n === 0) g[y][x] = 4; else if (n === 6) g[y][x] = 16;
  }
  return flat(g);
}

// ── map objects (transparent bg) ───────────────────────────────────────────────
function goldPile() {
  const g = blank();
  rectfill(g, 4, 10, 11, 13, 9);                             // base coins
  rectfill(g, 5, 7, 9, 10, 10); rectfill(g, 8, 8, 11, 11, 10);
  pixel(g, 6, 7, 23); pixel(g, 9, 9, 23);                   // glints
  rectfill(g, 6, 4, 8, 6, 10); pixel(g, 7, 4, 23);             // top coin
  return flat(outlined(g));
}
function orePile() {
  const g = blank();
  rectfill(g, 3, 9, 12, 13, 5);                              // rocks
  rectfill(g, 5, 6, 9, 10, 6); rectfill(g, 8, 8, 11, 11, 6);
  pixel(g, 6, 6, 7); pixel(g, 9, 8, 7);                     // facets
  rectfill(g, 6, 11, 9, 13, 25);                             // ore glow (orange seam)
  return flat(outlined(g));
}
function chest() {
  const g = blank();
  rectfill(g, 2, 7, 13, 14, 4);                              // body
  rectfill(g, 2, 4, 13, 7, 20);                              // lid
  rectfill(g, 2, 7, 13, 7, 10);                              // gold band
  rectfill(g, 7, 8, 8, 11, 10);                              // lock
  pixel(g, 4, 5, 23); pixel(g, 11, 5, 23);                  // highlights
  return flat(outlined(g));
}
function mine() {
  const g = blank();
  rectfill(g, 1, 6, 14, 14, 5);                              // mountain face
  rectfill(g, 4, 8, 11, 14, 16);                             // cave mouth (dark)
  rectfill(g, 5, 9, 10, 14, 0);
  rectfill(g, 9, 1, 9, 6, 6);                                // flagpole
  rectfill(g, 10, 1, 14, 4, 28);                             // banner (magic → owner color)
  rectfill(g, 6, 12, 7, 13, 25);                             // ore cart spark
  return flat(outlined(g));
}

// ── town / castle (transparent; magic 28 walls, 29 roofs, 7 windows) ────────────
function town() {
  const g = blank();
  rectfill(g, 1, 9, 14, 15, 28);                             // curtain wall
  rectfill(g, 1, 7, 3, 9, 28);  rectfill(g, 12, 7, 14, 9, 28);    // corner towers
  rectfill(g, 6, 5, 9, 9, 28);                               // keep
  rectfill(g, 1, 6, 3, 6, 29);  rectfill(g, 12, 6, 14, 6, 29);    // tower roofs
  rectfill(g, 6, 3, 9, 4, 29);                               // keep roof (peak)
  rectfill(g, 7, 1, 8, 2, 29);
  // crenellations
  for (let x = 1; x <= 14; x += 2) pixel(g, x, 9, 29);
  rectfill(g, 6, 11, 9, 15, 16);                             // gate
  rectfill(g, 7, 12, 8, 15, 25);                             // gate glow
  rectfill(g, 2, 7, 2, 7, 7); rectfill(g, 13, 7, 13, 7, 7);       // tower windows
  rectfill(g, 7, 6, 8, 7, 7);                                // keep window
  return flat(outlined(g));
}

// ── hero on the overworld (magic 28 cloak, 29 trim) ─────────────────────────────
function hero() {
  const g = blank();
  rectfill(g, 6, 2, 9, 4, 29);                               // helm/hat
  rectfill(g, 6, 5, 9, 7, 15);                               // face
  rectfill(g, 5, 8, 10, 12, 28);                             // cloak/body
  rectfill(g, 4, 8, 4, 11, 28); rectfill(g, 11, 8, 11, 11, 28);   // arms
  rectfill(g, 12, 3, 13, 9, 28);                             // banner cloth
  rectfill(g, 11, 2, 11, 12, 5);                             // banner pole
  rectfill(g, 6, 13, 7, 15, 5); rectfill(g, 8, 13, 9, 15, 5);     // legs
  return flat(outlined(g));
}

// ── battle obstacle rock (opaque-ish, no team color) ────────────────────────────
function rock() {
  const g = blank();
  rectfill(g, 2, 6, 13, 14, 5);
  rectfill(g, 4, 4, 11, 8, 6);
  pixel(g, 5, 5, 7); pixel(g, 9, 6, 7);
  rectfill(g, 3, 13, 12, 14, 16);
  return flat(outlined(g));
}

// ── creatures (transparent; magic 28 body, 29 dark, 15 skin, 5/6 metal) ─────────
function pikeman() {
  const g = blank();
  rectfill(g, 5, 4, 9, 5, 29);                               // helm
  rectfill(g, 6, 6, 8, 8, 15);                               // face
  rectfill(g, 5, 9, 9, 13, 28);                              // tunic
  rectfill(g, 4, 9, 4, 12, 28); rectfill(g, 10, 9, 10, 12, 28);   // arms
  rectfill(g, 12, 0, 12, 14, 6);                             // pike shaft
  rectfill(g, 11, 0, 13, 1, 7);                              // pike head
  rectfill(g, 5, 14, 6, 15, 5); rectfill(g, 8, 14, 9, 15, 5);     // legs
  rectfill(g, 4, 10, 11, 11, 29);                            // belt
  return flat(outlined(g));
}
function archer() {
  const g = blank();
  rectfill(g, 5, 3, 9, 4, 29);                               // hood
  rectfill(g, 6, 5, 8, 7, 15);                               // face
  rectfill(g, 5, 8, 9, 13, 28);                              // tunic
  rectfill(g, 4, 8, 4, 11, 28);                              // draw arm
  // bow (curved)
  const bow = [[11, 4], [12, 5], [12, 6], [13, 7], [13, 8], [12, 9], [12, 10], [11, 11]];
  for (const [bx, by] of bow) pixel(g, bx, by, 4);
  for (let y = 4; y <= 11; y++) pixel(g, 10, y, 6);       // bowstring
  rectfill(g, 5, 7, 11, 7, 23);                              // arrow
  rectfill(g, 5, 14, 6, 15, 5); rectfill(g, 8, 14, 9, 15, 5);     // legs
  return flat(outlined(g));
}
function griffin() {
  const g = blank();
  // wings spread
  rectfill(g, 0, 3, 4, 9, 6);  rectfill(g, 11, 3, 15, 9, 6);
  rectfill(g, 1, 4, 3, 8, 7);  rectfill(g, 12, 4, 14, 8, 7);
  rectfill(g, 5, 6, 10, 12, 25);                             // tawny body
  rectfill(g, 6, 3, 9, 6, 23);                               // eagle head (pale)
  rectfill(g, 9, 4, 11, 5, 9);                               // beak
  pixel(g, 8, 4, 0);                                      // eye
  rectfill(g, 5, 12, 6, 15, 25); rectfill(g, 9, 12, 10, 15, 25);  // legs
  pixel(g, 5, 15, 10); pixel(g, 10, 15, 10);                // talons
  rectfill(g, 6, 9, 9, 11, 28);                              // (magic) saddle/banner accent
  return flat(outlined(g));
}
function ogre() {
  const g = blank();
  rectfill(g, 4, 4, 11, 12, 27);                             // big green body (will read as brute)
  rectfill(g, 5, 2, 10, 5, 27);                              // head
  pixel(g, 6, 3, 8); pixel(g, 9, 3, 8);                     // red eyes
  rectfill(g, 6, 4, 9, 4, 16);                               // brow
  rectfill(g, 6, 5, 9, 5, 7);                                // tusks/teeth
  rectfill(g, 3, 6, 4, 10, 27); rectfill(g, 11, 6, 12, 10, 27);   // arms
  rectfill(g, 1, 8, 4, 11, 5);                               // club shaft
  rectfill(g, 0, 6, 3, 10, 6);                               // club head
  rectfill(g, 5, 12, 7, 15, 27); rectfill(g, 8, 12, 10, 15, 27);  // legs
  rectfill(g, 5, 9, 10, 11, 28);                             // (magic) loincloth → team tint
  return flat(outlined(g));
}

module.exports = {
  screenW: 320, screenH: 200, scale: 4,
  cellW: 16, cellH: 16, mapW: 128, mapH: 64,
  sprites: {
    1: grass(), 2: forest(), 3: mountain(), 4: water(), 5: road(),
    6: goldPile(), 7: orePile(), 8: chest(), 9: mine(),
    10: town(), 11: hero(),
    12: battlefield(), 13: rock(),
    16: pikeman(), 17: archer(), 18: griffin(), 19: ogre(),
  },
};
