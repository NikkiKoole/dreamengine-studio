// config for bleedblue.c — the other half of the BLEED RIG (bleedred + bleedblue).
// Slot 0 is a BLUE fish, deliberately a different shape + palette from bleedred's red
// critter. On switch, bleedblue draws plain (no pal/fillp) — so if the red cart's
// palette remap or fill pattern survives the switch, THIS cart shows it. The bleed rig's
// "clean canvas": anything red / patterned / critter-shaped here is a leak.
const { blank, pixel, rectfill, circlefill, outlined, flat } = require('../sprite-draw.js')

const BLUE = 12, LIGHT = 6, WHITE = 7, BLACK = 0

// a blue fish facing right — clearly not a round red critter
function fish() {
  const g = blank()
  circlefill(g, 7, 8, 5, BLUE)           // body
  circlefill(g, 6, 7, 3, LIGHT)          // highlight
  // tail (triangle-ish, pointing left)
  rectfill(g, 1, 7, 3, 8, BLUE)
  pixel(g, 1, 6, BLUE); pixel(g, 1, 9, BLUE)
  pixel(g, 9, 6, WHITE); pixel(g, 9, 7, BLACK)   // eye toward +x
  return flat(outlined(g))
}

module.exports = {
  screenW: 320, screenH: 240, scale: 3,
  cellW: 16, cellH: 16,
  sprites: { 0: fish() },
}
