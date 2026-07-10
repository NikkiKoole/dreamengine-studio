/* de:meta
{
  "slug": "tinyjam",
  "title": "tinyjam — heart + notes logo",
  "status": "active",
  "created": "2026-06-29",
  "kind": ["tech-demo"],
  "teaches": ["software-rasterizer"],
  "description": "Internal asset cart: draws the Tinyjam logo — a glossy amber heart cradling a smooth cream beamed eighth-note pair, all from engine primitives (no bitmap-font glyphs); a one-line tweak swaps the note to berry-jam purple. Tweak the constants at the top and re-bake to generate variations."
}
de:meta */
// tinyjam — the Tinyjam logo, drawn for image generation.
//
// Concept: a glossy amber heart (rim-shadow + sheen) cradling a smooth beamed
// eighth-note PAIR in berry-jam purple — every curve rasterised from engine
// primitives, no CGA/CP437 glyph. This is a TOOL cart: edit the TWEAKABLES
// below and re-bake to spit out a fresh logo image.
//
//   node tools/make-cart.js tools/carts/tinyjam.c editor/public/carts/tinyjam.cart.png   # build
//   node tools/make-cart.js --run editor/public/carts/tinyjam.cart.png                   # screenshot
//   # → look at build/.bake/tinyjam/screenshot.png
//
// Screen size is set in tinyjam.cart.js (128x128). Deterministic (no rnd/time).
#include "studio.h"
#include <math.h>

// ── TWEAKABLES ──────────────────────────────────────────────────────────────
#define BG          0               // background (0 = black)
#define HEART       CLR_ORANGE      // amber heart body
#define HEART_RIM   CLR_DARK_ORANGE // shadow rim under the heart (gives it volume)
#define HEART_GLOSS CLR_LIGHT_PEACH // soft sheen on the upper-left lobe
#define SPECULAR    CLR_WHITE       // tiny catch-light dot
#define NOTE        CLR_WHITE       // note colour (cream). Try CLR_DARK_PURPLE for a berry-jam look.
#define NOTE_SHADOW CLR_DARK_ORANGE // soft drop-shadow under the note (lift, not a hard outline)
#define HEART_R     58              // heart half-size in px
#define HEART_CX    64              // heart centre X (128-wide canvas)
#define HEART_CY    60              // heart centre Y (heart mass sits high)
#define NOTE_CX     64              // note-group centre X
#define NOTE_CY     54              // note-group centre Y (up in the heart's mass)
#define NOTE_U      10.0f           // note unit size (everything scales off this)
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

// A filled ellipse rotated by `ang` radians — the music-note head (a tilted oval,
// which a circle or axis-aligned ovalfill can't give us).
static void ellipse_rot_fill(int cx, int cy, float a, float b, float ang, int color) {
  float c = cosf(ang), s = sinf(ang);
  int R = (int)ceilf(a > b ? a : b) + 1;
  for (int py = -R; py <= R; py++)
    for (int px = -R; px <= R; px++) {
      float lx =  c * px + s * py;                // rotate sample into the ellipse's own frame
      float ly = -s * px + c * py;
      if ((lx * lx) / (a * a) + (ly * ly) / (b * b) <= 1.0f) pset(cx + px, cy + py, color);
    }
}

// A filled quad (two triangles) — the beam joining the two stems.
static void beam_quad(int x0,int y0,int x1,int y1,int x2,int y2,int x3,int y3,int col){
  trifill(x0,y0, x1,y1, x2,y2, col);
  trifill(x0,y0, x2,y2, x3,y3, col);
}

// A beamed eighth-note pair, centred on (gx,gy), scaled by unit u, drawn in `col`.
// Two tilted oval heads, two stems rising from their right shoulders, one slanted beam.
static void beamed_pair(int gx, int gy, float u, int col) {
  float a = 1.0f * u, b = 0.68f * u, ang = -0.38f;    // head oval + jaunty tilt
  int sw = (int)(0.30f * u + 0.5f); if (sw < 2) sw = 2;  // stem thickness

  int hAx = gx - (int)(1.5f * u), hAy = gy + (int)(1.0f * u);   // left head (lower)
  int hBx = gx + (int)(1.1f * u), hBy = gy + (int)(0.2f * u);   // right head (higher)
  int sAx = hAx + (int)(0.78f * u), sBx = hBx + (int)(0.78f * u);  // stems hug the right shoulder
  int topA = gy - (int)(2.3f * u), topB = gy - (int)(2.7f * u);    // stem tops (beam slants down-left)
  int bt = (int)(0.58f * u);                           // beam thickness (downward)

  thickline(sAx, hAy, sAx, topA, sw, col);
  thickline(sBx, hBy, sBx, topB, sw, col);
  beam_quad(sAx - sw/2, topA, sBx + sw/2, topB, sBx + sw/2, topB + bt, sAx - sw/2, topA + bt, col);
  ellipse_rot_fill(hAx, hAy, a, b, ang, col);
  ellipse_rot_fill(hBx, hBy, a, b, ang, col);
}

void draw(void) {
  cls(BG);

  const int cx = HEART_CX, cy = HEART_CY;

  // Glossy heart: shadow rim (shifted down) → body (shifted up) → sheen → catch-light.
  heartfill(cx, cy + 2, HEART_R,     HEART_RIM);
  heartfill(cx, cy - 1, HEART_R - 3, HEART);
  ovalfill(cx - 19, cy - 17, 10, 6, HEART_GLOSS);
  circfill(cx - 21, cy - 18, 2, SPECULAR);

  // The beamed-note mark — soft drop-shadow first (offset down-right), then the note.
  if (NOTE_SHADOW != HEART) beamed_pair(NOTE_CX + 2, NOTE_CY + 3, NOTE_U, NOTE_SHADOW);
  beamed_pair(NOTE_CX, NOTE_CY, NOTE_U, NOTE);
}
