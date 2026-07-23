// config for boxjelly.c — a COMBINED sheet: puppet body-part sprites (from
// puppet.cart.js, rows 0-3 of the sheet) PLUS jelly's face/checker discs relocated
// to slots 32..36 (sheet row 4, y=64) so the blob faces and the puppet parts never
// overlap. boxjelly.c reads the puppet parts at their standard rects and the blob
// faces at (skin*16, 64).
const puppet = require('./puppet.cart.js')
const jelly  = require('./jelly.cart.js')

const faces = {}
for (let s = 0; s < 5; s++) faces[32 + s] = jelly.sprites[s]   // slots 32..36 → sheet (s*16, 64)

module.exports = { sprites: { ...puppet.sprites, ...faces } }
