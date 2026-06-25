// config for bunnymark.c — FIVE pre-tinted bunny sprites in slots 0..4, the
// classic Pixi/Flash bunnymark "wabbit". Each is a flat coloured silhouette that
// reads fine at 16px among thousands.
//
// Why five baked sprites instead of one + pal() recolor: bunnymark is a pure
// SPRITE-BLIT throughput test, and pal() is the opposite of cheap here. On the
// GPU it forces a BeginShaderMode/EndShaderMode (a batch flush) around EVERY
// spr(), so 1000+ bunnies become 1000+ unbatched draw calls; on the software
// canvas it runs a per-pixel sw_recolor on every sprite. Baking the colour in
// means the draw loop is just spr(tint, x, y) — no pal() — so both backends
// measure the blit and nothing else. (Measured: pal() made GPU ~5× SLOWER than
// software; baking the tints fixes both. See docs/design/software-canvas.md.)

const { blank, pixel, rectfill, circlefill, ovalfill, outlined, flat } =
  require('../sprite-draw.js')

const EYE = 16   // CLR_BROWNISH_BLACK — matches the outline outlined() adds

// one bunny silhouette in body colour `c` (eye + outline stay dark)
function bunny(c) {
  const g = blank()
  rectfill(g, 4, 1, 5, 8, c)      // left ear
  rectfill(g, 10, 1, 11, 8, c)    // right ear
  circlefill(g, 8, 8, 4, c)       // head
  ovalfill(g, 8, 12, 5, 3, c)     // body
  pixel(g, 6, 7, EYE)             // eye
  return flat(outlined(g))
}

// slots 0..4 — keep in sync with the bunny tint index in bunnymark.c
//   0 white · 1 yellow · 2 pink · 3 lime-green · 4 orange
module.exports = {
  sprites: {
    0: bunny(7),    // CLR_WHITE
    1: bunny(10),   // CLR_YELLOW
    2: bunny(14),   // CLR_PINK
    3: bunny(26),   // CLR_LIME_GREEN
    4: bunny(9),    // CLR_ORANGE
  },
}
