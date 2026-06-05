// fontbake.cart.js — title text baked from a real TTF (Google Font "Bungee")
// at build time via tools/font-bake.js. No runtime font code: the rasterized
// words land in the sprite sheet like any hand-drawn sprite, so the C side
// just sspr()s a fixed sheet region (and pal() still recolors it).
//
// Slot layout (8-col sheet) — each word is centered in a fixed slot-rect so
// the C cart can hardcode the sheet region regardless of the word's width:
//   slots 0–15  "DREAM"  → sheet region (0,  0) 128×32
//   slots 16–23 "ENGINE" → sheet region (0, 32) 128×16

const { bakeBanner } = require('../font-bake.js')

const FONT = 'fonts/Bungee-Regular.ttf'

// title: yellow, orange AA edge, near-black outline, purple drop shadow
const title = bakeBanner(FONT, 'DREAM', 8, 2, { color: 10, aa: 9, outline: 16, shadow: 2 })
// subtitle: white with a dark-blue outline, no shadow — border alone
const sub = bakeBanner(FONT, 'ENGINE', 8, 1, { color: 7, outline: 17 })

const sprites = {}
title.forEach((t, i) => { sprites[((i / 8) | 0) * 8 + (i % 8)] = t })
sub.forEach((t, i) => { sprites[16 + i] = t })

module.exports = { sprites }
