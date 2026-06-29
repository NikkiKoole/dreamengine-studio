/* de:meta
{
  "title": "tinyjam — wordmark logo generator",
  "status": "active",
  "created": "2026-06-29",
  "kind": ["tech-demo"],
  "teaches": ["software-rasterizer"],
  "description": "Internal asset cart: draws the Tinyjam wordmark — 'tiny' (FONT_TINY) stacked over 'JAM' (FONT_NORMAL) inside an amber heart — for baking a small logo. Tweak the constants at the top and re-bake to generate variations."
}
de:meta */
// tinyjam — the Tinyjam wordmark/logo, drawn for image generation.
//
// Concept: "tiny" in the tiny font, stacked over "JAM" in the normal dos font,
// inside an amber heart, ~64x64. This is a TOOL cart: edit the TWEAKABLES below
// and re-bake to spit out a fresh logo image.
//
//   node tools/make-cart.js tools/carts/tinyjam.c /tmp/tinyjam.cart.png   # build
//   node tools/make-cart.js --run /tmp/tinyjam.cart.png                   # screenshot
//   # → look at build/.bake/tinyjam/screenshot.png
//
// Screen size is set in tinyjam.cart.js (64x64). Deterministic (no rnd/time).
#include "studio.h"

// ── TWEAKABLES ──────────────────────────────────────────────────────────────
#define BG        0            // background (0 = black)
#define HEART     CLR_ORANGE   // amber heart (palette 9)
#define INK       0            // wordmark colour (0 = black, reads on the amber)
#define TOP_TEXT  "tiny"       // small line (FONT_TINY 3x5)
#define BOT_TEXT  "JAM"        // big line (FONT_NORMAL 8x8) — try "JAMS" too
#define LOBE_R    12           // heart lobe radius
#define TOP_DY    (-10)        // "tiny" vertical offset from centre
#define BOT_DY    (-2)         // "JAM"  vertical offset from centre
// ─────────────────────────────────────────────────────────────────────────────

void draw(void) {
  cls(BG);

  const int cx = SCREEN_W / 2;
  const int cy = SCREEN_H / 2;

  // amber heart = two lobes (circles) + a bottom triangle
  circfill(cx - 10, cy - 6, LOBE_R, HEART);
  circfill(cx + 10, cy - 6, LOBE_R, HEART);
  trifill(cx - 21, cy - 2, cx + 21, cy - 2, cx, cy + 22, HEART);

  // wordmark, each line horizontally centred via text_width()
  font(FONT_TINY);
  print(TOP_TEXT, cx - text_width(TOP_TEXT) / 2, cy + TOP_DY, INK);
  font(FONT_NORMAL);
  print(BOT_TEXT, cx - text_width(BOT_TEXT) / 2, cy + BOT_DY, INK);
}
