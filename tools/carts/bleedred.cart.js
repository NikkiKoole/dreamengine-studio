// config for bleedred.c — half of the multi-cart BLEED RIG (bleedred + bleedblue).
// Slot 0 is a fat RED critter; the sibling cart's slot 0 is a BLUE fish. Both bundle
// with matching dims. If a cart switch doesn't swap the sprite sheet, bleedblue's
// spr(0) draws THIS red critter instead of its fish — the sprite-sheet bleed, visible.
const { blank, pixel, rectfill, circlefill, outlined, flat } = require('../sprite-draw.js')

const RED = 8, WHITE = 7, DARKRED = 2, BLACK = 0

// a round red critter with two white eyes + little feet — unmistakably NOT a fish
function critter() {
  const g = blank()
  circlefill(g, 8, 8, 6, RED)
  circlefill(g, 8, 9, 6, DARKRED)        // belly shade
  circlefill(g, 8, 7, 5, RED)
  pixel(g, 6, 6, WHITE); pixel(g, 10, 6, WHITE)   // eyes
  pixel(g, 6, 7, BLACK); pixel(g, 10, 7, BLACK)   // pupils
  rectfill(g, 5, 13, 6, 14, DARKRED)     // feet
  rectfill(g, 10, 13, 11, 14, DARKRED)
  return flat(outlined(g))
}

module.exports = {
  screenW: 320, screenH: 240, scale: 3,
  cellW: 16, cellH: 16,
  sprites: { 0: critter() },
}
