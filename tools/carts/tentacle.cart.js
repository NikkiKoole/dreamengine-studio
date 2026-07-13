// config for tentacle.c — three 16x16 SCALE TILES the cart maps along its bone ribbons
// with tritex(). Each tile must be FULLY OPAQUE (no index-0 / colorkey pixels) or the
// textured mesh would show see-through holes. The pattern is a brick-offset field of
// little scales so it reads as skin and tiles cleanly down a segmented tentacle.
//   slot 0 sea-green · slot 1 magenta · slot 2 deep-blue
// Iterate: node tools/sprite-preview.js tentacle   (then Read the PNG).

const { blank, circlefill, pixel, flat } = require('../sprite-draw.js')

function scales(base, scale, hi) {
  const g = blank(16, 16, base)                 // opaque base — no transparent pixels
  for (let row = -1; row <= 4; row++) {
    const y = row * 4
    const off = (row & 1) ? 4 : 0               // brick offset every other row
    for (let x = -1; x <= 2; x++) {
      const cx = x * 8 + off
      circlefill(g, cx, y, 3, scale)            // a scale
      pixel(g, cx, y - 1, hi)                    // a tiny highlight glint
    }
  }
  return flat(g)                                 // builder wants a flat 256-array, not the 2D grid
}

// candy VERTICAL STRIPES (4px) — the stripes run down the tentacle and shear as it twists
function vstripes(a, b) {
  const g = blank(16, 16, a)
  for (let y = 0; y < 16; y++)
    for (let x = 0; x < 16; x++)
      if ((x >> 2) & 1) pixel(g, x, y, b)
  return flat(g)
}

// CHECKERBOARD (4px cells) — the clearest read of the affine texture warp as it bends
function checker(a, b) {
  const g = blank(16, 16, a)
  for (let y = 0; y < 16; y++)
    for (let x = 0; x < 16; x++)
      if (((x >> 2) + (y >> 2)) & 1) pixel(g, x, y, b)
  return flat(g)
}

module.exports = {
  sprites: {
    0: scales(3, 11, 10),   // sea-green  (dark-green base, green scale, yellow glint)
    1: scales(2, 14, 15),   // magenta    (dark-purple base, pink scale, peach glint)
    2: scales(1, 12, 6),    // deep-blue  (dark-blue base, blue scale, light-grey glint)
    3: vstripes(7, 8),      // white / red candy stripes
    4: checker(7, 5),       // white / dark-grey checkerboard
  },
}
