// config for icons12.c — a little sheet of hand-usable 12×12 PICO-palette ICONS,
// drawn programmatically with sprite-draw.js. Each icon is authored on a 12×12
// canvas then centered into the 16×16 sprite slot (offset 2,2) so it packs into
// the normal 8×8 sheet — spr() it and you get a clean 12×12 glyph with 2px of
// transparent breathing room on every side.
//
// Slot layout:
//   0 smiley  ·  1 acid-house smiley  ·  2 steam locomotive  ·  3 palm tree
//
// pico32 indices used: 0 black · 3 dk-green · 4 brown · 5 dk-grey · 6 lt-grey ·
// 7 white · 8 red · 9 orange · 10 yellow · 11 green · 12 blue · 15 tan/peach ·
// 16 brownish-black (outline) · 20 dk-brown · 24 dk-red · 28 dk-blue.

const { blank, pixel, rectfill, line, circlefill, ovalfill, trifill, stamp, outlined, flat } =
  require('../sprite-draw.js')

// Author on 12×12, drop centered into a 16×16 slot, then trace an outline that
// may bleed 1px into the padding. Keeps every icon a true 12×12 glyph.
function icon12(draw, outline = true) {
  const g = blank(12, 12)
  draw(g)
  const slot = blank(16, 16)
  stamp(slot, g, 2, 2)
  return flat(outline ? outlined(slot) : slot)
}

// ── 0: friendly smiley ─────────────────────────────────────────────────────────
function ic_smiley() {
  return icon12((g) => {
    circlefill(g, 6, 6, 5.6, 10)                 // yellow face
    circlefill(g, 6, 6, 5.6, 10)
    // eyes — two small black dots
    rectfill(g, 3, 4, 4, 5, 0)
    rectfill(g, 7, 4, 8, 5, 0)
    // gentle upturned smile
    pixel(g, 3, 7, 0); pixel(g, 8, 7, 0)
    rectfill(g, 4, 8, 7, 8, 0)
    // a little cheek warmth
    pixel(g, 2, 6, 9); pixel(g, 9, 6, 9)
  })
}

// ── 1: acid-house smiley (bold, iconic grin, tall oval eyes) ────────────────────
function ic_acid() {
  return icon12((g) => {
    circlefill(g, 6, 6, 5.6, 10)                 // bright yellow face
    circlefill(g, 6, 6, 5.6, 10)
    // eyes — tall black ovals, the classic acid look
    rectfill(g, 3, 3, 4, 5, 0)
    rectfill(g, 7, 3, 8, 5, 0)
    // big banana grin, thick and turned right up at the ends
    pixel(g, 2, 6, 0); pixel(g, 3, 7, 0)
    pixel(g, 9, 6, 0); pixel(g, 8, 7, 0)
    rectfill(g, 3, 8, 8, 9, 0)                    // fat lower lip
    rectfill(g, 4, 7, 7, 7, 0)                    // upper edge of the grin
  })
}

// ── 2: steam locomotive (side view, front to the left) ──────────────────────────
function ic_train() {
  return icon12((g) => {
    // smoke puffs drifting up-left off the stack
    circlefill(g, 2, 1, 1, 6)
    pixel(g, 1, 0, 6)
    // smokestack + steam dome sitting on the boiler
    rectfill(g, 2, 3, 3, 4, 0)                    // stack body
    rectfill(g, 1, 2, 3, 2, 0)                    // flared cap
    rectfill(g, 5, 3, 6, 3, 6)                    // steam dome
    // boiler barrel
    rectfill(g, 1, 4, 7, 6, 5)
    line(g, 1, 4, 7, 4, 6)                        // top highlight
    // cab at the back
    rectfill(g, 8, 2, 10, 6, 24)                  // dark-red cab body
    rectfill(g, 7, 2, 10, 2, 0)                   // roof overhang
    rectfill(g, 8, 3, 9, 4, 12)                   // blue window
    // running board / footplate across the base
    rectfill(g, 0, 7, 11, 7, 5)
    // driving wheels + a small leading wheel, hubs picked out in grey
    circlefill(g, 3, 9, 1.7, 0); pixel(g, 3, 9, 6)
    circlefill(g, 7, 9, 1.7, 0); pixel(g, 7, 9, 6)
    circlefill(g, 10, 9, 1.1, 0)
  })
}

// ── 3: palm tree ────────────────────────────────────────────────────────────────
function ic_palm() {
  return icon12((g) => {
    // 2px-wide curved trunk, leaning slightly right, root to crown
    line(g, 4, 11, 5, 5, 4); line(g, 5, 11, 6, 5, 4)
    pixel(g, 4, 11, 20); pixel(g, 5, 9, 20); pixel(g, 5, 7, 20)   // segment shading
    // crown: five drooping fronds radiating from (5,4), two greens for depth
    line(g, 5, 4, 0, 2, 11); line(g, 0, 2, 0, 4, 3)     // far-left, droops
    line(g, 5, 4, 2, 0, 11)                             // upper-left
    line(g, 5, 4, 6, 0, 11)                             // top
    line(g, 5, 4, 9, 0, 11)                             // upper-right
    line(g, 5, 4, 11, 2, 11); line(g, 11, 2, 11, 4, 3)  // far-right, droops
    // broaden the fronds so they read as leaves, not wires
    pixel(g, 1, 2, 3); pixel(g, 2, 2, 11); pixel(g, 3, 1, 11)
    pixel(g, 7, 1, 11); pixel(g, 8, 2, 3); pixel(g, 9, 2, 11)
    pixel(g, 4, 1, 11); pixel(g, 6, 1, 3)
    // coconuts tucked under the crown
    pixel(g, 4, 5, 4); pixel(g, 7, 5, 4)
    // little sandy mound at the base
    line(g, 3, 11, 8, 11, 15)
  })
}

module.exports = {
  screenW: 320, screenH: 200, scale: 4,
  cellW: 16, cellH: 16,
  sprites: {
    0: ic_smiley(),
    1: ic_acid(),
    2: ic_train(),
    3: ic_palm(),
  },
}
