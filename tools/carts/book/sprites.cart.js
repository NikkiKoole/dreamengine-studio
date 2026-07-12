// sprites for the book's Chapter 6 (spr). Five 16x16 sprites drawn in code with
// sprite-draw.js. Slot 0/1 = a two-frame walk; 2 = coin; 3 = heart; 4 = bush.
// Index 0 (black) is the transparent colorkey; outlines use 16.
// Iterate: node tools/sprite-preview.js book/sprites   (then Read the PNG).
const { blank, pixel, rectfill, line, circlefill, ovalfill, trifill,
        outlined, flat } = require('../../sprite-draw.js')

function hero(step) {                     // a little adventurer facing right
  const g = blank()
  rectfill(g, 5, 2, 10, 3, 4)             // brown hair/cap
  rectfill(g, 5, 4, 10, 7, 31)            // peach face
  pixel(g, 6, 5, 16); pixel(g, 9, 5, 16)  // eyes
  rectfill(g, 5, 8, 10, 11, 12)           // blue body
  rectfill(g, 3, 8, 4, 10, 31)            // arms
  rectfill(g, 11, 8, 12, 10, 31)
  if (step === 0) { rectfill(g, 5, 12, 7, 15, 5); rectfill(g, 8, 12, 10, 15, 5) }
  else            { rectfill(g, 4, 12, 6, 15, 5); rectfill(g, 9, 12, 11, 14, 5) }
  return flat(outlined(g))
}

function coin() {
  const g = blank()
  circlefill(g, 8, 8, 5, 10)              // yellow disc
  circlefill(g, 8, 8, 3, 9)               // orange inner
  pixel(g, 6, 6, 7)                       // shine
  return flat(outlined(g))
}

function heart() {
  const g = blank()
  circlefill(g, 5, 6, 3, 8)
  circlefill(g, 10, 6, 3, 8)
  trifill(g, 2, 7, 13, 7, 8, 14, 8)
  pixel(g, 5, 5, 7)                       // glint
  return flat(outlined(g))
}

function bush() {
  const g = blank()
  ovalfill(g, 8, 11, 7, 4, 3)             // dark-green base
  ovalfill(g, 6, 8, 4, 3, 11)             // green lobes
  ovalfill(g, 10, 8, 4, 3, 11)
  ovalfill(g, 8, 6, 4, 3, 11)
  return flat(outlined(g))
}

module.exports = {
  screenW: 320, screenH: 200, scale: 4,
  cellW: 16, cellH: 16,
  sprites: { 0: hero(0), 1: hero(1), 2: coin(), 3: heart(), 4: bush() },
}
