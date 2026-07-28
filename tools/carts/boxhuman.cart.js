// config for boxhuman.c — a humanoid drawn as FOUR continuous sprite strips
// (head, torso, arm, leg) laid out on the 128x128 sheet. Each strip spans
// several Box2D bones and is deformed by them, so a whole arm is ONE texture
// that bends at the shoulder AND the elbow AND the wrist instead of three
// rigid quads with seams.
//
// Left/right limbs REUSE the same strip (the cart mirrors the geometry and
// leaves the UVs alone), so the sheet only holds one arm and one leg.
//
// Rects the cart reads (keep in sync with SKINS[] in boxhuman.c):
//   head  ( 0, 0, 20, 20)
//   torso (24, 0, 28, 38)
//   arm   (56, 0, 14, 50)   shoulder at top, hand at bottom
//   leg   (76, 0, 20, 62)   hip at top, foot pointing RIGHT at the bottom
//
// Every limb is BANDED across its axis, and that is the whole point of the art:
// a flat colour tells you nothing about how a skin deformed, while a regular
// stripe fans out on the outside of a bend, bunches on the inside, and shears
// visibly if a blend mode gets it wrong. The pink bands mark the joints; the
// narrower stripes between them read the stretch in between.
//
// Iterate: node tools/sprite-preview.js boxhuman  (then Read the PNG).

const { blank, circlefill, rrectfill, rectfill, polyfill, pixel, split } =
  require('../sprite-draw.js')

// pico32 indices
const SHIRT = 12   // blue      — torso + upper-arm sleeve
const SKIN = 15    // peach     — face, forearm, hands
const PANTS = 28   // true blue  — hips + thighs (NOT palette 1: that IS the backdrop)
const SOCK = 6     // light grey— lower leg
const SHOE = 4     // brown     — foot
const HAIR = 4     // brown
const CUFF = 14    // pink      — joint landmark bands
const EYE = 16     // brownish-black — NOT 0: the cart colorkeys index 0 away
// stripe partners, picked for contrast against their base at 320x200
const SHIRT2 = 7   // white     — on SHIRT
const PANTS2 = 12  // blue      — on PANTS
const SOCK2 = 8    // red       — on SOCK
const SKIN2 = 30   // dark peach— on SKIN
const SHOE2 = 20   // dark brown— on SHOE
const HAIR2 = 20   // dark brown— on HAIR

// Recolour only pixels that already hold `from` — lets a rect/box shape a
// region without spilling outside the silhouette that's already there.
function recolor(g, x0, y0, x1, y1, from, to) {
  for (let y = y0; y <= y1; y++)
    for (let x = x0; x <= x1; x++)
      if (g[y] && g[y][x] === from) g[y][x] = to
}

// Band `base` pixels with `alt` in stripes of `period` px. Only recolours pixels
// that already hold `base`, so the silhouette is untouched. horiz = bands run
// ACROSS a vertical limb (the useful direction); pass false for the foot, which
// runs sideways.
function stripes(g, x0, y0, x1, y1, base, alt, period, horiz = true, phase = 0) {
  for (let y = y0; y <= y1; y++)
    for (let x = x0; x <= x1; x++) {
      if (!g[y] || g[y][x] !== base) continue
      const n = Math.floor(((horiz ? y : x) + phase) / period)
      if (n % 2 === 1) g[y][x] = alt
    }
}

function figure() {
  const g = blank(128, 128, 0)

  // ── head (0,0,20,20) ───────────────────────────────────────────────
  circlefill(g, 10, 10, 8, SKIN)
  recolor(g, 0, 0, 19, 6, SKIN, HAIR)   // hair cap, clipped to the head circle
  pixel(g, 7, 11, EYE)
  pixel(g, 13, 11, EYE)
  rectfill(g, 8, 15, 12, 15, EYE)       // mouth

  // ── torso (24,0,28,44) ─────────────────────────────────────────────
  // wide shoulders, pinched waist, hips flare again. The shorts run 6px PAST
  // the hip joints on purpose: they overlap the tops of both leg strips, which
  // is what hides the hip seam (and the sliver of backdrop between the legs)
  // no matter how the legs swing. Cutout overlap, the cheap answer.
  polyfill(g, [
    34, 0, 42, 0,      // neck stump
    51, 4, 51, 14,     // right shoulder
    45, 22,            // right waist
    50, 30, 50, 40,    // right hip
    26, 40, 26, 30,    // left hip
    31, 22,            // left waist
    25, 14, 25, 4,     // left shoulder
  ], SHIRT)
  recolor(g, 24, 28, 51, 40, SHIRT, PANTS)   // trunks: the hip line reads
  rectfill(g, 34, 10, 42, 11, CUFF)          // chest band — watch the twist
  stripes(g, 24, 0, 51, 27, SHIRT, SHIRT2, 3)      // breton torso
  stripes(g, 24, 28, 51, 40, PANTS, PANTS2, 3)     // banded shorts

  // ── arm (56,0,14,50) ───────────────────────────────────────────────
  // one strip: shoulder ball -> sleeve -> ELBOW -> forearm -> WRIST -> hand
  circlefill(g, 63, 5, 5, SHIRT)
  rrectfill(g, 59, 2, 9, 20, 4, SHIRT)
  rectfill(g, 59, 21, 67, 23, CUFF)
  rrectfill(g, 60, 22, 7, 18, 3, SKIN)
  rectfill(g, 60, 38, 66, 39, CUFF)
  circlefill(g, 63, 44, 5, SKIN)
  stripes(g, 56, 0, 69, 21, SHIRT, SHIRT2, 3)      // sleeve bands
  stripes(g, 56, 24, 69, 37, SKIN, SKIN2, 3)       // forearm bands
  stripes(g, 56, 40, 69, 49, SKIN, SKIN2, 2)       // hand, finer

  // ── leg (76,0,20,62) ───────────────────────────────────────────────
  // one strip: hip ball -> thigh -> KNEE -> shin -> ANKLE -> foot (points RIGHT)
  circlefill(g, 82, 5, 5, PANTS)
  rrectfill(g, 77, 2, 11, 26, 5, PANTS)
  rectfill(g, 77, 27, 88, 29, CUFF)
  rrectfill(g, 78, 28, 9, 22, 4, SOCK)
  rectfill(g, 78, 48, 87, 49, CUFF)
  polyfill(g, [78, 49, 87, 49, 95, 57, 95, 61, 78, 61], SHOE)
  stripes(g, 76, 0, 95, 26, PANTS, PANTS2, 3)      // thigh bands
  stripes(g, 76, 30, 95, 47, SOCK, SOCK2, 3)       // striped socks
  stripes(g, 76, 49, 95, 61, SHOE, SHOE2, 3, false) // foot: bands run the OTHER way,
                                                    // because the foot runs sideways
  stripes(g, 0, 0, 19, 8, HAIR, HAIR2, 3)          // hair, so head twist reads

  return g
}

const t = split(figure())
module.exports = { sprites: Object.fromEntries(t.map((s, i) => [i, s])) }
