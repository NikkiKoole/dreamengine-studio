// config for puppet.c — six side-view BODY-PART sprites laid out at known sheet
// rects. The cart reads each part's alpha with sget(), convex-hulls the opaque
// pixels down to <=8 (x,y) points (Box2D's B2_MAX_POLYGON_VERTICES), and feeds
// that to b2ComputeHull/b2MakePolygon — one convex physics poly per part — then
// textures the poly with tritex. Arms + legs reuse one sprite for left & right.
// Iterate: node tools/sprite-preview.js puppet   (then Read the PNG).

const { blank, circlefill, rectfill, rrectfill, polyfill, pixel, line, split } = require('../sprite-draw.js')

// palette (all NON-ZERO — index 0 is transparent = "outside the part")
const SKIN = 15, HAIR = 4, SHIRT = 12, PANTS = 13, SHOE = 5, DARK = 0 + 5, EYE = 0 + 5

function sheet() {
  const g = blank(128, 128, 0)

  // HEAD  rect (0,0,16,16) — round head + face
  circlefill(g, 8, 9, 7, SKIN)
  rrectfill(g, 1, 1, 14, 6, 3, HAIR)          // hair cap
  pixel(g, 5, 9, EYE); pixel(g, 11, 9, EYE)   // eyes
  line(g, 6, 12, 10, 12, EYE)                 // smile

  // TORSO rect (16,0,20,34) — a shirt (slight taper via two stacked rrects)
  rrectfill(g, 18, 1, 16, 12, 4, SHIRT)       // shoulders
  rrectfill(g, 19, 10, 14, 22, 3, SHIRT)      // belly
  pixel(g, 26, 8, DARK); pixel(g, 26, 14, DARK); pixel(g, 26, 20, DARK)  // buttons

  // UPPER ARM rect (40,0,10,24) — sleeve capsule
  rrectfill(g, 41, 1, 8, 22, 4, SHIRT)

  // LOWER ARM rect (52,0,10,22) — bare forearm + a fist at the far end
  rrectfill(g, 53, 1, 8, 18, 4, SKIN)
  circlefill(g, 57, 19, 3, SKIN)

  // UPPER LEG rect (64,0,12,28) — trouser
  rrectfill(g, 65, 1, 10, 26, 4, PANTS)

  // LOWER LEG rect (78,0,12,26) — trouser + a shoe
  rrectfill(g, 79, 1, 10, 20, 4, PANTS)
  rrectfill(g, 79, 19, 11, 6, 2, SHOE)

  // ── SECOND RIG: a desk lamp (proves the same engine drives a different sheet) ──
  const METAL = 5, METAL2 = 6, GLOW = 10
  // LAMP BASE rect (0,40,24,14) — a heavy trapezoid foot + a riser stub
  polyfill(g, [2, 53, 22, 53, 19, 47, 5, 47], METAL)   // trapezoid foot
  rrectfill(g, 8, 41, 8, 8, 2, METAL2)                 // riser
  // LAMP ARM rect (26,40,8,34) — a solid rod
  rrectfill(g, 27, 41, 6, 32, 2, METAL2)
  // LAMP HEAD rect (36,40,22,14) — a lampshade (wide at the bottom) + a bulb glow
  polyfill(g, [44, 41, 50, 41, 57, 48, 37, 48], METAL) // shade cone
  rrectfill(g, 42, 47, 10, 4, 2, GLOW)                 // bulb glow
  pixel(g, 46, 51, GLOW); pixel(g, 47, 51, GLOW)

  return g
}

// 128×128 → 8×8 grid of 16×16 slots, row-major → slots 0..63
const tiles = split(sheet())
module.exports = { sprites: Object.fromEntries(tiles.map((t, i) => [i, t])) }
