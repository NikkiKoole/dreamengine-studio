// squishy — 320×320 square canvas at 3× (960×960 window), plus 11 little
// tool-icon sprites (slots 0..10, matching the BRUSHES table order) drawn with
// sprite-draw.js. They're ink-on-paper glyphs: the cart keys out black (idx 0) so
// the shapes read on the paper cell. pico32 indices: 4 brown · 5 dk-grey
// · 6 lt-grey · 7 white · 8 red · 9 orange · 10 yellow · 12 blue · 14 pink ·
// 16 brownish-black (our INK) · 20 dk-brown · 31 peach.
const { blank, pixel, line, rectfill, rrectfill, circlefill, ovalfill, trifill, flat } =
  require('../sprite-draw.js')

const K = 16   // INK (brownish-black)

// 0 — ink brush: brown handle + a fat black ink tip
function ic_ink() {
  const g = blank()
  // solid diagonal handle (a filled band — single lines left gaps)
  trifill(g, 13, 1, 15, 3, 7, 11, 4); trifill(g, 13, 1, 7, 11, 5, 9, 4)
  rectfill(g, 5, 9, 8, 11, 6)                                                   // ferrule
  circlefill(g, 4, 12, 3, K)                                                    // ink tip
  pixel(g, 2, 14, K); pixel(g, 6, 14, K)                                        // drips
  return flat(g)
}
// 1 — pencil: yellow body, wooden tip, pink eraser
function ic_pen() {
  const g = blank()
  line(g, 13, 3, 5, 11, 10); line(g, 14, 4, 6, 12, 10); line(g, 12, 2, 4, 10, 10)  // body
  trifill(g, 3, 13, 6, 10, 6, 13, 20)                                              // wood tip
  pixel(g, 3, 13, K); pixel(g, 2, 13, K)                                           // graphite
  rectfill(g, 12, 2, 14, 4, 14)                                                    // eraser
  line(g, 11, 4, 13, 2, 6)                                                         // ferrule
  return flat(g)
}
// 2 — fineliner: slim blue pen, fine tip
function ic_fin() {
  const g = blank()
  line(g, 13, 3, 5, 11, 12); line(g, 14, 4, 6, 12, 12)   // thin blue body
  pixel(g, 4, 12, K); pixel(g, 3, 13, K)                 // fine tip
  rectfill(g, 12, 2, 14, 4, 6)                           // grey cap (white would vanish on paper)
  return flat(g)
}
// 3 — marker: fat orange body, grey collar, broad chisel tip
function ic_mrk() {
  const g = blank()
  rrectfill(g, 5, 2, 11, 8, 2, 9)        // orange body
  rectfill(g, 6, 8, 10, 10, 6)           // collar
  trifill(g, 5, 10, 11, 10, 8, 14, K)    // chisel tip
  pixel(g, 9, 3, 7)                      // highlight
  return flat(g)
}
// 4 — chalk: a dusty grey stick on an angle, with grain
function ic_chk() {
  const g = blank()
  line(g, 3, 12, 11, 4, 6); line(g, 4, 13, 12, 5, 6); line(g, 4, 12, 12, 4, 5)   // grey stick
  pixel(g, 11, 4, 5); pixel(g, 12, 5, 5)                                          // worn end
  pixel(g, 6, 9, K); pixel(g, 8, 7, K); pixel(g, 9, 8, K)                         // grain
  return flat(g)
}
// 5 — sketch: a loose ink scribble
function ic_skt() {
  const g = blank()
  line(g, 3, 9, 7, 4, K); line(g, 7, 4, 11, 9, K); line(g, 11, 9, 5, 11, K)
  line(g, 5, 11, 12, 6, K); line(g, 12, 6, 4, 8, K)
  return flat(g)
}
// 6 — spray can: grey can + dots spraying up-right
function ic_spr() {
  const g = blank()
  rrectfill(g, 4, 7, 9, 14, 1, 6)        // can body
  rectfill(g, 5, 5, 8, 7, 5)             // cap
  pixel(g, 6, 4, K)                      // nozzle
  pixel(g, 9, 4, K); pixel(g, 11, 3, K); pixel(g, 10, 6, K)   // spray
  pixel(g, 12, 5, K); pixel(g, 13, 2, K); pixel(g, 11, 7, K)
  return flat(g)
}
// 7 — bristle brush: brown handle + splayed dark bristles
function ic_brs() {
  const g = blank()
  line(g, 13, 2, 8, 8, 4); line(g, 14, 3, 9, 9, 4)   // handle
  rectfill(g, 6, 8, 9, 10, 6)                         // ferrule
  line(g, 7, 10, 4, 15, K); line(g, 8, 10, 6, 15, K); line(g, 8, 10, 8, 15, K)
  line(g, 9, 10, 10, 15, K); line(g, 9, 10, 12, 15, K)   // splayed bristles
  return flat(g)
}
// 8 — paint: a wet colour band up top with runs dripping down (beads at the tips)
function ic_paint() {
  const g = blank()
  const C = 12   // blue paint (reads on the paper cell; yellow would vanish)
  rrectfill(g, 2, 2, 13, 6, 1, C)                              // paint band
  line(g, 4, 6, 4, 11, C);  circlefill(g, 4, 12, 1, C)         // long run + bead
  line(g, 8, 6, 8, 8, C);   circlefill(g, 8, 9, 1, C)          // short run
  line(g, 11, 6, 11, 13, C); circlefill(g, 11, 14, 1, C)       // longest run
  return flat(g)
}
// 9 — calligraphy nib: a broad-nib pen tip + a thick/thin angled stroke
function ic_nib() {
  const g = blank()
  line(g, 13, 2, 6, 9, 5); line(g, 14, 3, 7, 10, 5)   // grey pen shaft
  trifill(g, 5, 9, 8, 10, 6, 12, K)                    // the flat nib tip
  // a calligraphic mark: thick on the down-diagonal, thin on the cross
  line(g, 2, 15, 4, 13, K); line(g, 2, 14, 5, 13, K); line(g, 3, 15, 5, 14, K)
  return flat(g)
}
// nib family shaft + flat tip (shared with ic_nib) — variants differ only in the MARK below
function nib_body(g) {
  line(g, 13, 2, 6, 9, 5); line(g, 14, 3, 7, 10, 5)   // grey pen shaft
  trifill(g, 5, 9, 8, 10, 6, 12, K)                    // the flat nib tip
}
// 11 — brush pen (SPEED): nib + a swelling comma mark (fat, tapering to a point)
function ic_bpn() {
  const g = blank(); nib_body(g)
  circlefill(g, 2, 15, 2, K)                           // fat start
  line(g, 3, 14, 5, 13, K); pixel(g, 6, 13, K)         // taper to a thin point
  return flat(g)
}
// 12 — reed pen (JITTER): nib + a chattering zigzag mark (dry, ragged angle)
function ic_red() {
  const g = blank(); nib_body(g)
  line(g, 1, 15, 2, 13, K); line(g, 2, 13, 3, 15, K)
  line(g, 3, 15, 4, 13, K); line(g, 4, 13, 5, 15, K); line(g, 5, 15, 6, 13, K)
  return flat(g)
}
// 13 — twist nib (SPIN): nib + a coil mark (angle winds along the stroke)
function ic_twt() {
  const g = blank(); nib_body(g)
  line(g, 2, 15, 2, 12, K); line(g, 2, 12, 6, 12, K)   // squared spiral / coil
  line(g, 6, 12, 6, 15, K); line(g, 6, 15, 4, 15, K)
  return flat(g)
}
// 10 — oil / impasto: a palette knife smearing a thick beveled blob of paint
function ic_oil() {
  const g = blank()
  line(g, 14, 2, 8, 8, 6); line(g, 13, 2, 7, 8, 5)     // knife handle
  trifill(g, 4, 8, 9, 8, 6, 12, 6)                      // the flat blade
  // a thick paint smear with a lit top edge (HILITE) + shadow底 (impasto ridge)
  rrectfill(g, 2, 11, 12, 15, 1, 9)                     // orange paint body
  line(g, 2, 11, 12, 11, 10)                            // lit ridge (light yellow)
  line(g, 3, 15, 13, 15, K)                             // shadow groove (impasto edge)
  return flat(g)
}

// Slots 0..8 are the brush icons (one per BRUSHES[] entry, matching order). The colour /
// dither / select-tool graphics are now drawn procedurally in squishy.c (rectfill / fillp /
// a dashed marquee box), so their old sprites (8..21) are gone — nothing loads them.
const sprites = {
  0: ic_ink(), 1: ic_pen(),  2: ic_fin(), 3: ic_mrk(),   4: ic_chk(),  5: ic_skt(),
  6: ic_spr(), 7: ic_brs(),  8: ic_paint(), 9: ic_nib(), 10: ic_oil(),
  11: ic_bpn(), 12: ic_red(), 13: ic_twt(),
}

module.exports = { screenW: 320, screenH: 320, scale: 3, sprites }
