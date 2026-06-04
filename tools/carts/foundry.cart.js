// foundry.cart.js — the construction-snapshot bake for SPRITE FOUNDRY.
//
// The cart plays back HOW a sprite gets drawn: after every drawing step the
// canvas is snapshotted into the next sprite slot, so the sheet holds each
// subject's own time-lapse and the C side just flips through slots.
//
// Three subjects, three different money shots of the toolkit:
//   DRAGON  slots  0-9    shade() rolls light over flat fills, outlined()
//   SHIP    slots 10-13   mirror() symmetry, then rotations(g,8) bakes 8
//           slots 14-21   headings (outlined AFTER rotating — runtime
//                         spr_rot can't do that)
//   BOSS    slots 22-29   noise() warts, replace() bake-time recolor,
//           slots 40,41,  then scale2x(): the 16x16 sketch becomes a
//                 48,49   smoothed 32x32 spread over a 2x2 slot block
//
// KEEP IN SYNC: foundry.c hardcodes the slot layout above and one label
// per snapshot. Add/remove a snap() here -> update the step tables there.

const { blank, pixel, rectfill, rrectfill, circlefill, ovalfill, trifill,
        polyfill, noise, shade, rotations, scale2x, replace,
        outlined, mirror, flat, split } = require('../sprite-draw.js')

const sprites = {}
let slot = 0
const snap = g => { sprites[slot++] = flat(g) }

// ── subject 1: DRAGON — flat shapes, then shade() lights them ── slots 0-9 ──
{
  let g = blank()
  ovalfill(g, 8, 10, 5, 3.2, 11); snap(g)                          // body
  trifill(g, 0, 6, 3, 12, 7, 12, 11); snap(g)                      // tail
  circlefill(g, 12, 5, 2.6, 11); snap(g)                           // head
  rectfill(g, 14, 4, 15, 6, 11); snap(g)                           // snout
  rectfill(g, 5, 13, 6, 15, 3)
  rectfill(g, 10, 13, 11, 15, 3); snap(g)                          // legs
  ovalfill(g, 8, 11.5, 3, 1.6, 26); snap(g)                        // belly
  trifill(g, 3, 8, 4, 4, 6, 8, 3)
  trifill(g, 6, 7, 7, 3, 9, 7, 3); snap(g)                         // back spikes
  pixel(g, 12, 4, 7); pixel(g, 13, 4, 16); pixel(g, 15, 5, 3); snap(g) // face
  g = shade(g); snap(g)                                            // light it
  g = outlined(g); snap(g)                                         // border
}

// ── subject 2: SHIP — mirror() then rotations() ── slots 10-13 + 14-21 ──────
{
  let g = blank()
  polyfill(g, [7.9, 0.5, 4.5, 6, 5.5, 13, 7.9, 13.5], 6); snap(g)  // hull, LEFT half
  trifill(g, 6, 7, 1, 13, 6, 12, 12); snap(g)                      // wing, left
  mirror(g); snap(g)                                               // -> symmetric
  ovalfill(g, 8, 5, 1.4, 2.2, 12)
  pixel(g, 7, 14, 9); pixel(g, 8, 14, 9); snap(g)                  // canopy + flame
  // 8 baked headings, each outlined AFTER rotating (slots 14-21)
  rotations(g, 8).forEach(r => snap(outlined(r)))
}

// ── subject 3: BOSS — noise/replace, then scale2x ── slots 22-29 + 2x2 @40 ──
{
  let g = blank()
  rrectfill(g, 2, 2, 12, 10, 3, 27); snap(g)                       // skull
  rrectfill(g, 1, 9, 14, 6, 2, 27); snap(g)                        // jaw
  trifill(g, 0, 6, 1, 1, 4, 4, 6)
  trifill(g, 15, 6, 14, 1, 11, 4, 6); snap(g)                      // horns
  rectfill(g, 4, 5, 5, 6, 10); rectfill(g, 10, 5, 11, 6, 10)
  pixel(g, 5, 6, 24); pixel(g, 10, 6, 24); snap(g)                 // angry eyes
  rectfill(g, 4, 11, 11, 11, 16)
  for (const x of [4, 6, 8, 10]) pixel(g, x, 11, 7)
  snap(g)                                                          // mouth + teeth
  for (let y = 0; y < 16; y++)
    for (let x = 0; x < 16; x++)
      if (g[y][x] === 27 && noise(x, y, 7) === 0) g[y][x] = 3
  snap(g)                                                          // noise() warts
  replace(g, 27, 29); replace(g, 3, 18); snap(g)                   // recolor: mauve
  g = outlined(g); snap(g)                                         // border
  const tiles = split(scale2x(g))                                  // 32x32 -> 4 slots
  sprites[40] = tiles[0]; sprites[41] = tiles[1]                   // top row
  sprites[48] = tiles[2]; sprites[49] = tiles[3]                   // bottom row
}

module.exports = { screenW: 320, screenH: 200, scale: 3, sprites }
