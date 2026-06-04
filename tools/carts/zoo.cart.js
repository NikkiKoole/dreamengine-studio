// config for zoo.c — sprites that get rotated with spr_rot().
// slot 0 = visitor (kid), slot 1 = balloon, slot 2 = pinwheel, slot 3 = sun.
// 1/2/3 are generated as raw 16x16 palette-index arrays so the geometry is exact
// (cart.js sprites accept raw index arrays, not just ASCII art).

const { blank, pixel, circlefill, trifill, flat } = require('../sprite-draw.js')

// slot 2 — pinwheel: four swept blades around a white hub (Y=10 O=9 R=8 g=11 W=7)
const pinwheel = blank()
trifill(pinwheel, 8, 8, 2, 1, 9, 3, 10)   // yellow blade, up
trifill(pinwheel, 8, 8, 15, 2, 13, 9, 9)  // orange blade, right
trifill(pinwheel, 8, 8, 14, 15, 7, 13, 8) // red blade, down
trifill(pinwheel, 8, 8, 1, 14, 3, 7, 11)  // green blade, left
circlefill(pinwheel, 8, 8, 1.6, 7)        // hub

// slot 3 — sun: orange rays radiating from a yellow core
const sun = blank()
for (let k = 0; k < 8; k++) {
  const a = k * Math.PI / 4
  const ex = 8 + Math.cos(a) * 7.5, ey = 8 + Math.sin(a) * 7.5
  const bx1 = 8 + Math.cos(a + 0.32) * 3.5, by1 = 8 + Math.sin(a + 0.32) * 3.5
  const bx2 = 8 + Math.cos(a - 0.32) * 3.5, by2 = 8 + Math.sin(a - 0.32) * 3.5
  trifill(sun, bx1, by1, bx2, by2, ex, ey, 9)
}
circlefill(sun, 8, 8, 4.2, 10) // yellow core

// slots 1,4,5,6,7 — balloons in five colors (spr_rot ignores pal(), so we bake one
// sprite per color). white glint + pink knot on each.
function balloonOf(body) {
  const g = blank()
  circlefill(g, 8, 7, 6, body)
  circlefill(g, 6, 5, 1.4, 7)
  pixel(g, 7, 13, 14); pixel(g, 8, 13, 14); pixel(g, 7, 14, 14)
  return flat(g)
}
const balloonRed = balloonOf(8), balloonOrange = balloonOf(9),
      balloonGreen = balloonOf(11), balloonBlue = balloonOf(12), balloonPink = balloonOf(14)

module.exports = {
  sprites: {
    // slot 0 — visitor: brown hair, peach face, dark-blue eyes (so colorkey(black)
    // doesn't punch holes), green tee, blue shorts, brown shoes
    0: `
................
......NNNN......
.....NNNNNN.....
.....NppppN.....
.....pppppp.....
.....pBppBp.....
......pppp......
.....gggggg.....
....gggggggg....
...pgggggggg p..
....gggggggg....
.....bbbbbb.....
.....bb..bb.....
.....bb..bb.....
....NNN..NNN....
................
`,
    1: balloonRed,
    2: flat(pinwheel),
    3: flat(sun),
    4: balloonOrange,
    5: balloonGreen,
    6: balloonBlue,
    7: balloonPink,
  },
}
