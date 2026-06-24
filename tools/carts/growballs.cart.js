// growballs.cart.js — sprites for Grow Balls, all code-drawn with sprite-draw.js.
//
//   slots 0–15   "GROW BALLS" title banner, baked from the HairyBeard TTF
//                (a furry display face) → sheet region (0,0) 128×32.
//   slot 16      the little guy (body + head + spiky hair) — facing right;
//                the C side sprf()-flips him to face left.
//   slots 17–21  fruit & veg: apple, carrot, grapes, broccoli, banana.
//
// The BALLS are NOT a sprite — they grow continuously (r 3→34) and are drawn as
// primitive ovals in C so they stay smooth at any size (a stretched sprite goes
// blocky). Same for the giant moon and the variable-width platforms.

const { bakeBanner } = require('../font-bake.js')
const { blank, pixel, rectfill, rrectfill, line, circlefill, ovalfill,
        trifill, shade, outlined, flat } = require('../sprite-draw.js')

// palette indices
const SKIN = 15, HAIR = 4, DARK = 16, RED = 8, ORANGE = 9, YEL = 10,
      DGRN = 3, GRN = 11, GRAPE = 13, BROWN = 4, PEACH = 15, WHITE = 7

// ── the guy: just head + hair + torso. Arms, legs, balls and willy are drawn
// in C as floppy VERLET chains hanging off this body, so they flop as he moves.
function guy() {
  let g = blank()
  // torso
  rrectfill(g, 5, 7, 6, 6, 2, SKIN)
  // head
  circlefill(g, 8, 4, 3, SKIN)
  g = shade(g, 235)                       // soft volume on the skin
  // spiky hair on top + side tufts (added after shade so it stays solid brown)
  pixel(g, 5, 1, HAIR); pixel(g, 7, 0, HAIR); pixel(g, 9, 0, HAIR); pixel(g, 11, 1, HAIR)
  pixel(g, 6, 1, HAIR); pixel(g, 8, 1, HAIR); pixel(g, 10, 1, HAIR)
  pixel(g, 4, 3, HAIR); pixel(g, 12, 3, HAIR)
  // face
  pixel(g, 7, 4, DARK); pixel(g, 9, 4, DARK)        // eyes
  pixel(g, 7, 6, DARK); pixel(g, 8, 6, DARK); pixel(g, 9, 6, DARK)  // smile
  pixel(g, 8, 10, DARK)                             // belly button
  return flat(outlined(g))
}

// ── fruit & veg ──────────────────────────────────────────────────────────────
function apple() {
  let g = blank()
  circlefill(g, 8, 9, 4, RED)
  g = shade(g, 235)
  pixel(g, 8, 4, BROWN); pixel(g, 8, 3, BROWN)      // stalk
  pixel(g, 10, 4, GRN); pixel(g, 11, 4, GRN); pixel(g, 11, 3, GRN)  // leaf
  pixel(g, 6, 7, WHITE)                             // shine
  return flat(outlined(g))
}
function carrot() {
  let g = blank()
  trifill(g, 4, 6, 12, 6, 8, 14, ORANGE)
  g = shade(g, 235)
  line(g, 6, 5, 5, 2, GRN); line(g, 8, 5, 8, 1, GRN); line(g, 10, 5, 11, 2, GRN)
  return flat(outlined(g))
}
function grapes() {
  let g = blank()
  const G = GRAPE
  circlefill(g, 5, 7, 2, G); circlefill(g, 9, 7, 2, G)
  circlefill(g, 7, 9, 2, G); circlefill(g, 5, 11, 2, G); circlefill(g, 9, 11, 2, G)
  circlefill(g, 7, 13, 2, G)
  pixel(g, 4, 6, WHITE); pixel(g, 6, 8, WHITE)      // little shines (no shade — keeps grapes purple)
  pixel(g, 8, 4, BROWN); pixel(g, 8, 3, BROWN)
  pixel(g, 10, 4, GRN); pixel(g, 11, 4, GRN)
  return flat(outlined(g))
}
function broccoli() {
  let g = blank()
  circlefill(g, 5, 7, 3, DGRN); circlefill(g, 11, 7, 3, DGRN)
  circlefill(g, 8, 5, 3, DGRN); circlefill(g, 8, 8, 3, GRN)
  g = shade(g, 235)
  rectfill(g, 7, 10, 9, 14, GRN)                    // stalk
  return flat(outlined(g))
}
function banana() {
  let g = blank()
  // a fat crescent
  circlefill(g, 8, 9, 6, YEL)
  circlefill(g, 10, 6, 6, 0)                        // carve the inner curve away
  g = shade(g, 235)
  pixel(g, 3, 8, BROWN); pixel(g, 13, 11, BROWN)    // tips
  return flat(outlined(g))
}

const sprites = {
  16: guy(),
  17: apple(),
  18: carrot(),
  19: grapes(),
  20: broccoli(),
  21: banana(),
}

// title banner → slots 0–15 — coloured to match the guy: PEACH skin fill,
// BROWN hair-coloured outline (aa edge in orange, the darker skin shade).
bakeBanner('fonts/HairyBeard-8M8an.ttf', 'GROW BALLS', 8, 2,
           { color: PEACH, aa: ORANGE, outline: HAIR, shadow: 0 })
  .forEach((t, i) => { sprites[((i / 8) | 0) * 8 + (i % 8)] = t })

module.exports = { sprites }
