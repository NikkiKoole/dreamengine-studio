// config for jelly.c — three 16x16 DISC textures the cart fans across a soft-body blob
// with tritex(). Only the disc (radius ~7 around the centre) is ever sampled — the fan maps
// the hub to the tile centre and each rim point to a point on a circle — so the corners can
// stay transparent. Two cute faces + a checkerboard disc (the checker makes the squish read).
//   slot 0 green face · slot 1 pink face · slot 2 blue checker
// Iterate: node tools/sprite-preview.js jelly   (then Read the PNG).

const { blank, circlefill, pixel, flat } = require('../sprite-draw.js')

function face(body, cheek) {
  const g = blank(16, 16, 0)                    // transparent corners (never sampled)
  circlefill(g, 8, 8, 7, body)                  // the body disc
  pixel(g, 3, 9, cheek); pixel(g, 12, 9, cheek) // blush
  circlefill(g, 5, 6, 2, 7); circlefill(g, 11, 6, 2, 7)   // white eyes
  pixel(g, 5, 6, 5); pixel(g, 11, 6, 5)         // pupils (dark-grey, never index-0)
  pixel(g, 5, 11, 5); pixel(g, 6, 12, 5); pixel(g, 7, 12, 5)   // smile
  pixel(g, 8, 12, 5); pixel(g, 9, 12, 5); pixel(g, 10, 11, 5)
  return flat(g)
}

function checkerDisc(a, b) {
  const g = blank(16, 16, 0)
  for (let y = 0; y < 16; y++)
    for (let x = 0; x < 16; x++) {
      const dx = x - 8, dy = y - 8
      if (dx * dx + dy * dy <= 49)              // inside radius 7
        pixel(g, x, y, (((x >> 2) + (y >> 2)) & 1) ? b : a)
    }
  return flat(g)
}

module.exports = {
  sprites: {
    0: face(11, 14),        // green body, pink blush
    1: face(14, 8),         // pink body, red blush
    2: checkerDisc(7, 12),  // white / blue checker disc
  },
}
