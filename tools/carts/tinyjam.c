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
#define BG         0            // background (0 = black)
#define HEART      CLR_ORANGE   // amber heart (palette 9)
#define INK        0            // glyph colour (0 = black, reads on the amber)
// CP437 glyphs: ♥ "\x03"  ♪ "\x0d"  ♫ "\x0e"  ☼ "\x0f". Pair them e.g. "\x0d\x0e".
#define GLYPH      "\x0d"       // the mark inside the heart (single ♪ note)
#define NOTE_SCALE 5            // glyph size multiplier
#define HEART_R    58           // heart half-size in px
#define HEART_CY   58           // heart centre Y (heart mass sits high, so above mid)
#define NOTE_DY    (-20)        // glyph Y offset from heart centre (heart mass sits high)
#define NOTE_DX    (3)          // glyph X nudge (♪ ink is left-biased in its 8px cell)
// ─────────────────────────────────────────────────────────────────────────────

// One clean heart silhouette from the implicit curve (x^2+y^2-1)^3 - x^2*y^3 <= 0,
// rasterised per-pixel — uniform pixel edge, no circle/triangle seam.
static void heartfill(int cx, int cy, int s, int color) {
  for (int py = -s; py <= s; py++) {
    float y = -(float)py / (s * 0.80f);          // y points up
    for (int px = -s; px <= s; px++) {
      float x = (float)px / (s * 0.92f);
      float a = x * x + y * y - 1.0f;
      if (a * a * a - x * x * y * y * y <= 0.0f) pset(cx + px, cy + py, color);
    }
  }
}

void draw(void) {
  cls(BG);

  const int cx = SCREEN_W / 2;
  const int cy = HEART_CY;

  heartfill(cx, cy, HEART_R, HEART);

  // The mark (CP437 glyph, e.g. ♪ char 13), centred in the heart's upper mass.
  // The dos font renders control codes as glyphs (no newline handling).
  font(FONT_NORMAL);
  print_scaled(GLYPH, cx - (text_width(GLYPH) * NOTE_SCALE) / 2 + NOTE_DX, cy + NOTE_DY, INK, NOTE_SCALE);
}
