// highnoon.cart.js — every word in the duel is baked from the Google Font
// "Smokum" (the wanted-poster slab serif) by tools/font-bake.js. The words ARE
// the game's feedback: DRAW! is the start signal, TOO SOON / DEAD / YOU WIN
// the verdicts.
//
// Slot layout (8-col sheet), each word centered in a fixed slot-rect so the C
// side sspr()s constant sheet regions. Every word gets two slot-rows of height
// (one row = ~11px glyphs after outline trim, too thin for Smokum at 2x) — the
// two half-width verdicts share a row pair, packing the 64 slots exactly:
//   slots  0–15        "HIGH NOON" → sheet ( 0,  0) 128×32
//   slots 16–31        "DRAW!"     → sheet ( 0, 32) 128×32
//   slots 32–35/40–43  "DEAD"      → sheet ( 0, 64)  64×32
//   slots 36–39/44–47  "EARLY!"    → sheet (64, 64)  64×32
//   slots 48–63        "YOU WIN"   → sheet ( 0, 96) 128×32

const { bakeBanner } = require('../font-bake.js')

const FONT = 'fonts/Smokum-Regular.ttf'

const words = [
  // [text, slotCols, baseSlot, opts]
  ['HIGH NOON', 8, 0,  { color: 15, aa: 4, outline: 16, shadow: 20 }],  // peach, brown edge
  ['DRAW!',     8, 16, { color: 8, aa: 24, outline: 16, shadow: 20 }],  // red
  ['DEAD',      4, 32, { color: 8, aa: 24, outline: 16 }],              // red
  ['EARLY!',    4, 36, { color: 9, aa: 25, outline: 16 }],              // orange
  ['YOU WIN',   8, 48, { color: 10, aa: 9, outline: 16 }],              // yellow
]

const sprites = {}
for (const [text, cols, base, opts] of words)
  bakeBanner(FONT, text, cols, 2, opts).forEach((t, i) => {
    sprites[base + ((i / cols) | 0) * 8 + (i % cols)] = t
  })

module.exports = { sprites }
