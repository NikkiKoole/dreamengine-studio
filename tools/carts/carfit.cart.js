// config for carfit.c — one 64×32 side-view CAR sprite laid across the sheet so the
// cart can auto-trace it at runtime with sget(). split() cuts the wide canvas into
// eight 16×16 slots; placed at 0,1,2,3 (top row) + 8,9,10,11 (second row) the sheet
// region (0,0)-(63,31) reads back as one contiguous image.
//   Colours are all NON-ZERO on purpose: index 0 = transparent = "outside the car",
//   which is exactly the alpha mask the outline tracer + mesh clipper key off.
// Iterate: node tools/sprite-preview.js carfit   (then Read the PNG).

const { blank, rrectfill, circlefill, rectfill, split } = require('../sprite-draw.js')

function car() {
  const g = blank(64, 32, 0)                 // transparent = outside the silhouette

  // main body — a long rounded slab
  rrectfill(g, 3, 13, 58, 12, 4, 8)          // red body  (x3..60, y13..24)
  // cabin / roof
  rrectfill(g, 17, 4, 30, 12, 4, 8)          // raised cabin (x17..46, y4..15)
  // windows (light blue, split by a pillar)
  rrectfill(g, 20, 7, 11, 6, 1, 12)
  rrectfill(g, 34, 7, 10, 6, 1, 12)
  // side trim stripe
  rectfill(g, 4, 20, 59, 21, 14)             // pink beltline
  // headlight + tail light
  circlefill(g, 59, 16, 1, 10)               // yellow head lamp
  circlefill(g, 4, 16, 1, 8)                 // red tail
  // wheels — dark tyre + light hub, dropping below the body (concave underside between them)
  circlefill(g, 17, 25, 6, 5); circlefill(g, 17, 25, 3, 6)
  circlefill(g, 46, 25, 6, 5); circlefill(g, 46, 25, 3, 6)

  return g
}

// 64×32 → 4×2 tiles, row-major: [r0c0,r0c1,r0c2,r0c3, r1c0,r1c1,r1c2,r1c3]
const t = split(car())

module.exports = {
  sprites: { 0: t[0], 1: t[1], 2: t[2], 3: t[3], 8: t[4], 9: t[5], 10: t[6], 11: t[7] },
}
