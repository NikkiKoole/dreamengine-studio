// config for trackgen.c — a few tiny pixel-art HUD buttons (drawn in code with
// sprite-draw.js). trackgen renders its track/cars procedurally; the ONLY sprites
// are these cute in-drive toggle buttons, so the player taps an icon instead of
// hunting for a keyboard shortcut.
//
//   slot 0  CHASE  — a police light bar (red + blue)
//   slot 1  DRIFT  — a little car laying down skid marks
//   slot 2  DEBUG  — an eye (the why-overlay)
//
// pico32 indices used: 5 dk-grey · 6 lt-grey · 7 white · 8 red · 12 blue ·
// 16 brownish-black (outline) · 31 peach.

const { blank, pixel, rectfill, line, circlefill, ovalfill, rrectfill, outlined, flat } =
  require('../sprite-draw.js')

// CHASE — a cop light bar: two lamps (red, blue) on a dark mount.
function ic_chase() {
  const g = blank()
  rectfill(g, 2, 10, 13, 13, 5)                 // dark-grey light bar
  rectfill(g, 2, 13, 13, 14, 6)                 // base lip (lighter)
  circlefill(g, 5, 9, 2, 8)                      // red lamp
  circlefill(g, 10, 9, 2, 12)                    // blue lamp
  pixel(g, 5, 8, 31); pixel(g, 10, 8, 7)        // glints
  return flat(outlined(g))
}

// DRIFT — two parallel tyre skid marks (the universal drift/skid symbol).
function ic_drift() {
  const g = blank()
  // mark 1 (S-curve, black)
  line(g, 2, 13, 6, 8, 16); line(g, 6, 8, 10, 10, 16); line(g, 10, 10, 14, 4, 16)
  // mark 2 (parallel, offset down)
  line(g, 2, 15, 6, 10, 16); line(g, 6, 10, 10, 12, 16); line(g, 10, 12, 14, 6, 16)
  return flat(g)
}

// DEBUG — an eye (the inspect / why-overlay toggle).
function ic_debug() {
  const g = blank()
  ovalfill(g, 8, 8, 6, 4, 7)                     // white eyeball
  circlefill(g, 8, 8, 3, 12)                     // blue iris
  circlefill(g, 8, 8, 1, 16)                     // pupil
  pixel(g, 6, 6, 7)                              // glint
  return flat(outlined(g))
}

module.exports = {
  screenW: 320, screenH: 200, scale: 4,
  cellW: 16, cellH: 16,
  sprites: { 0: ic_chase(), 1: ic_drift(), 2: ic_debug() },
}
