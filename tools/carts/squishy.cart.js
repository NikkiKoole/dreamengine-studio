// squishy — 320×320 square canvas at 3× (960×960 window), plus 8 little
// tool-icon sprites (slots 0..7, matching the BRUSHES table order) drawn with
// sprite-draw.js. They're ink-on-paper glyphs: the dropdown buttons use a paper
// background so dark ink shapes read crisply. pico32 indices: 4 brown · 5 dk-grey
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

// 8..11 — solid colour swatches for the palette picker (must match COLORS[] in squishy.c)
const swatch = (c) => flat(blank(16, 16, c))

// 12.. — dither swatches: a Bayer-ordered DENSITY ramp (must match PATTERNS[] in squishy.c).
// Each is a 16-bit fillp mask (bit 15 = top-left cell; 0-bit = ink, 1-bit = hole/paper); decode
// the 4×4 tile and ink the 0-bits so the swatch shows exactly what the pattern fills.
const PAT_MASKS = [0x0000, 0x7FDF, 0x5F5F, 0x5A5A, 0x0A0A, 0x0208]   // solid · ~12 / 25 / 50 / 75 / 87%
function patSwatch(mask) {
  const g = blank()
  for (let y = 0; y < 16; y++) for (let x = 0; x < 16; x++) {
    const cell = (y % 4) * 4 + (x % 4)
    if (((mask >> (15 - cell)) & 1) === 0) pixel(g, x, y, K)   // 0-bit = ink
  }
  return flat(g)
}

const sprites = {
  0: ic_ink(), 1: ic_pen(), 2: ic_fin(), 3: ic_mrk(),
  4: ic_chk(), 5: ic_skt(), 6: ic_spr(), 7: ic_brs(),
  8: swatch(16), 9: swatch(12), 10: swatch(8), 11: swatch(3),     // ink / blue / red / dk-green
}
PAT_MASKS.forEach((m, i) => { sprites[12 + i] = patSwatch(m) })   // 12.. = the dither ramp

module.exports = { screenW: 320, screenH: 320, scale: 3, sprites }
