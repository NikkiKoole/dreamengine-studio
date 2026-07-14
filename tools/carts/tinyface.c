/* de:meta
{
  "slug": "tinyface",
  "title": "tiny face (160x100)",
  "status": "active",
  "created": "2026-07-14",
  "kind": [
    "probe"
  ],
  "teaches": [],
  "description": "A VIBE MOCKUP at HALF resolution — 160x100 scaled 4x — to test whether a tighter canvas strengthens the candy-toy device face by forcing the iconic essence. The slime mascot is the hero on a little green screen (the soul); a couple of chunky knobs, a fader with an amber LED, and a row of candy pads round it out. Every pixel earns its place. Draw-only + a touch of now() life. Mockups-first for vibe work; designed FOR the small canvas, not scaled down to it."
}
de:meta */
#include "studio.h"

// TINYFACE — the candy device face at 160x100 (half res, 4x). The constraint is
// the point: distil the toy to its iconic essence. Mascot = hero.

static void mnote(int x, int y, int col) { circfill(x, y + 4, 1, col); rectfill(x + 1, y, 1, 5, col); line(x + 2, y, x + 4, y - 1, col); }
static void heart(int cx, int cy, int col) { circfill(cx - 1, cy - 1, 1, col); circfill(cx + 1, cy - 1, 1, col); trifill(cx - 2, cy, cx + 2, cy, cx, cy + 2, col); }
static void sparkle(int x, int y, int col, float ph) { int s = (int)(0.5f + 1.2f * sin_deg(ph)); if (s < 1) return; line(x - s, y, x + s, y, col); line(x, y - s, x, y + s, col); }

// a chunky candy pad, squished when pressed
static void candy_pad(int x, int y, int w, int h, int face, int hi, int lo, int pressed) {
    blend(BLEND_AVG); rrectfill(x + 1, y + 2, w, h, 3, CLR_BLACK); blend_reset();
    int dy = pressed ? 1 : 0, hh = pressed ? h - 1 : h;
    rrectfill(x, y + dy, w, hh, 3, face);
    line(x + 2, y + dy + 1, x + w - 3, y + dy + 1, hi);
    line(x + 1, y + dy + 2, x + 1, y + dy + hh - 3, hi);
    blend(BLEND_AVG); line(x + 2, y + dy + hh - 1, x + w - 3, y + dy + hh - 1, lo); blend_reset();
    rrect(x, y + dy, w, hh, 3, CLR_BROWNISH_BLACK);
}
static void knob(int cx, int cy, int r, float v) {
    blend(BLEND_AVG); circfill(cx + 1, cy + 2, r, CLR_BLACK); blend_reset();
    circfill(cx, cy, r, CLR_INDIGO);
    ring(cx, cy, r - 2, r - 1, 165, 285, CLR_PINK);
    ring(cx, cy, r - 2, r - 1, -15, 105, CLR_DARKER_PURPLE);
    circ(cx, cy, r, CLR_BROWNISH_BLACK);
    float a = 150 + v * 240;
    line(cx + (int)dx(r * 0.4f, a), cy + (int)dy(r * 0.4f, a), cx + (int)dx(r - 2, a), cy + (int)dy(r - 2, a), CLR_WHITE);
}
// the SLIME MASCOT — the hero, bops + blinks
static void mascot(int cx, int cy, float t) {
    int bop = (int)(sin_deg(t * 260) * 2); cy += bop;
    ovalfill(cx, cy, 12, 10, CLR_GREEN);
    ovalfill(cx, cy + 2, 11, 7, CLR_LIME_GREEN);
    int blink = sin_deg(t * 90) > 0.85f;
    for (int e = -1; e <= 1; e += 2) { int ex = cx + e * 4, ey = cy - 2;
        if (blink) line(ex - 1, ey, ex + 1, ey, CLR_BROWNISH_BLACK);
        else { circfill(ex, ey, 2, CLR_WHITE); pset(ex, ey, CLR_BROWNISH_BLACK); } }
    arc(cx, cy + 2, 3, 20, 160, CLR_DARK_GREEN);
    circfill(cx - 6, cy + 1, 1, CLR_PINK); circfill(cx + 6, cy + 1, 1, CLR_PINK);
    mnote(cx + 12, cy - 8 - bop, CLR_LIGHT_PEACH);
}

void draw(void) {
    float t = now();
    // case: purple shell + cream chassis
    cls(CLR_DARK_PURPLE);
    rrectfill(0, 0, 160, 100, 8, CLR_INDIGO);
    rrectfill(4, 4, 152, 92, 6, CLR_LIGHT_PEACH);
    blend(BLEND_AVG); line(8, 5, 148, 5, CLR_WHITE); blend_reset();

    // title
    font(FONT_SMALL); print("TINY JAM", 9, 8, CLR_RED);
    heart(60, 10, CLR_RED);
    sparkle(78, 8, CLR_LIGHT_YELLOW, t * 220);

    // screen + mascot (the soul)
    rrectfill(8, 16, 80, 46, 4, CLR_BROWNISH_BLACK);
    rrectfill(11, 19, 74, 40, 3, CLR_DARK_GREEN);
    blend(BLEND_AVG); for (int y = 20; y < 59; y += 2) line(11, y, 84, y, CLR_BROWNISH_BLACK); blend_reset();
    mascot(40, 38, t);
    font(FONT_TINY); print("SLIME 1", 13, 54, CLR_MEDIUM_GREEN);

    // knobs + fader with amber LED
    knob(104, 30, 9, 0.35f); knob(126, 30, 9, 0.7f);
    font(FONT_TINY); print("CUT", 98, 42, CLR_DARK_BROWN); print("RES", 120, 42, CLR_DARK_BROWN);
    int fx = 148; rrectfill(fx - 1, 18, 4, 34, 2, CLR_BROWNISH_BLACK);
    rrectfill(fx - 4, 32, 9, 5, 1, CLR_MEDIUM_GREY);
    circfill(fx, 15, 1, CLR_ORANGE);

    // candy pad row
    int pc[4] = { CLR_PINK, CLR_ORANGE, CLR_YELLOW, CLR_GREEN };
    int ph[4] = { CLR_LIGHT_PEACH, CLR_LIGHT_YELLOW, CLR_LIGHT_YELLOW, CLR_LIME_GREEN };
    int pl[4] = { CLR_DARK_PURPLE, CLR_DARK_ORANGE, CLR_DARK_ORANGE, CLR_DARK_GREEN };
    int hit = ((int)(t * 4)) % 4;
    for (int i = 0; i < 4; i++) candy_pad(8 + i * 27, 68, 24, 24, pc[i], ph[i], pl[i], i == hit);
    sparkle(8 + hit * 27 + 12, 65, CLR_WHITE, t * 400);

    // big round play pad (blue)
    blend(BLEND_AVG); circfill(133, 80, 14, CLR_BLACK); blend_reset();
    circfill(132, 79, 14, CLR_TRUE_BLUE);
    ring(132, 79, 11, 13, 165, 285, CLR_BLUE);
    circ(132, 79, 14, CLR_BROWNISH_BLACK);
    trifill(127, 73, 127, 85, 139, 79, CLR_WHITE);

    font(FONT_NORMAL);
}
