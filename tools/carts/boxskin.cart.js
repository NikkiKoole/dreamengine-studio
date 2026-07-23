// config for boxskin.c — ONE continuous limb sprite (a 20x64 arm) laid at sheet
// (0,0). The cart skins this single texture across TWO Box2D bones (upper arm +
// forearm) joined at an elbow, so the paint bends smoothly at the joint instead of
// being two rigid halves. split() cuts the 128x128 sheet into 16x16 slots.
// Iterate: node tools/sprite-preview.js boxskin  (then Read the PNG).

const { blank, circlefill, rrectfill, rectfill, split } = require('../sprite-draw.js')

const SLEEVE = 12, SKIN = 15, CUFF = 14, NAIL = 5

function limb() {
  const g = blank(128, 128, 0)
  // ARM rect (0,0,20,64): shoulder ball + sleeved upper arm + bare forearm + hand
  circlefill(g, 10, 6, 6, SLEEVE)          // shoulder ball
  rrectfill(g, 3, 3, 14, 28, 5, SLEEVE)    // upper-arm sleeve
  rectfill(g, 4, 28, 15, 30, CUFF)         // cuff band (a landmark to watch bend at the elbow)
  rrectfill(g, 4, 30, 12, 26, 4, SKIN)     // forearm
  circlefill(g, 10, 58, 5, SKIN)           // hand
  circlefill(g, 10, 60, 2, NAIL)           // knuckles dot
  return g
}

const t = split(limb())
module.exports = { sprites: Object.fromEntries(t.map((s, i) => [i, s])) }
