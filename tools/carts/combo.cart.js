// combo.cart.js — settings + the baked "COMBO" amp-badge logo.
// The badge is baked from the Google Font "Bungee" (chunky display slab) by
// tools/font-bake.js into slots 0–15 (sheet region 0,0 → 128×32), drawn with a
// single sspr() in combo.c's draw(). combo.c MUST colorkey(0) in init() so the
// palette-0 padding around the word is transparent (else it's an opaque box).

const { bakeBanner } = require('../font-bake.js')

const FONT = 'fonts/Bungee-Regular.ttf'

const sprites = {}
bakeBanner(FONT, 'COMBO', 8, 2, { color: 10, aa: 9, outline: 16, shadow: 20 })   // brass yellow, orange edge
  .forEach((tile, i) => { sprites[i] = tile })

module.exports = { screenW: 320, screenH: 240, scale: 3, sprites }
