// citydrive wall textures — four tileable 16x16 facades for tritex-mapped walls.
// Each is authored to tile seamlessly: bricks/blocks repeat on an 8px grid,
// windows on an 8px grid, so a wall subdivided into 16px tiles reads continuous.
// Slots: 0 brick · 1 concrete block · 2 window facade · 3 stucco.
const { blank, rectfill, line, pixel, flat } = require('../sprite-draw.js');

// running-bond red brick: 8px-wide bricks, 4px rows, half-brick offset per row,
// 1px mortar gutters. Two warm tones so it isn't a flat slab.
function tex_brick() {
  const g = blank(16, 16, 16);                 // mortar = brownish-black (16)
  for (let r = 0; r < 4; r++) {
    const oy = r * 4, off = (r % 2) ? 4 : 0;
    for (let bx = off - 8; bx < 16; bx += 8) {
      const c = ((r + (bx >> 3)) & 1) ? 4 : 25; // brown / dark-orange
      rectfill(g, bx, oy + 1, bx + 6, oy + 3, c);
    }
  }
  return flat(g);
}

// concrete block: 8x8 blocks, stack bond, grey mortar grid, two grey tones.
function tex_block() {
  const g = blank(16, 16, 5);                   // mortar = dark grey (5)
  for (let r = 0; r < 2; r++)
    for (let bx = 0; bx < 16; bx += 8) {
      const c = ((r + (bx >> 3)) & 1) ? 6 : 22; // light grey / medium grey
      rectfill(g, bx + 1, r * 8 + 1, bx + 6, r * 8 + 6, c);
    }
  return flat(g);
}

// office window facade: dark spandrel + a 2x2 grid of windows, a few lit.
function tex_glass() {
  const g = blank(16, 16, 17);                  // wall = darker blue (17)
  let n = 0;
  for (let r = 0; r < 2; r++)
    for (let c = 0; c < 2; c++) {
      const wx = 2 + c * 8, wy = 2 + r * 8;
      const lit = (n++ % 3) === 0;
      rectfill(g, wx, wy, wx + 3, wy + 4, lit ? 23 : 12); // lit yellow / blue glass
      pixel(g, wx, wy, 28);                     // top-left frame glint
    }
  return flat(g);
}

// stucco / sandstone: warm flat wall with faint score-lines + speckle.
function tex_stucco() {
  const g = blank(16, 16, 31);                  // peach (31)
  line(g, 0, 5, 15, 5, 30);                     // dark-peach score lines (30)
  line(g, 0, 11, 15, 11, 30);
  pixel(g, 3, 2, 15); pixel(g, 11, 8, 15);      // light-peach flecks (15)
  pixel(g, 7, 13, 30); pixel(g, 13, 3, 30);
  return flat(g);
}

module.exports = {
  screenW: 320, screenH: 200, scale: 4,
  sprites: {
    0: tex_brick(),
    1: tex_block(),
    2: tex_glass(),
    3: tex_stucco(),
  },
};
