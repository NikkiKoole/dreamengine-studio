/* de:meta
{
  "slug": "facemock",
  "title": "face mock (candy)",
  "status": "active",
  "created": "2026-07-14",
  "kind": [
    "probe"
  ],
  "teaches": [],
  "description": "A VIBE MOCKUP (not functional) for the device-face look — chasing the TinyJam icon come to life: cream chassis in a purple shell, a green LCD with a little slime MASCOT that bops (the soul), candy pads, a pulsing heart, sparkles, chunky purple knobs, a fader with an amber LED, a speaker grille. Draw-only + a touch of now() life so we can tweak the candy-toy finish cheaply before porting it into grooveface. Mockups-first for vibe work."
}
de:meta */
#include "studio.h"

// FACEMOCK — a static VIBE mockup. The point is CHARM, not function: make the
// device face look like the TinyJam icon come alive (candy chassis + a mascot
// with soul on the screen + juice). Tweak here, port the finish to grooveface.

// ── little charm primitives ────────────────────────────────────────────────
static void heart(int cx, int cy, int r, int col) {
    circfill(cx - r / 2, cy - r / 3, r / 2 + 1, col);
    circfill(cx + r / 2, cy - r / 3, r / 2 + 1, col);
    trifill(cx - r, cy - r / 4, cx + r, cy - r / 4, cx, cy + r, col);
    pset(cx - r / 2, cy - r / 2, CLR_WHITE);                 // glint
}
static void mnote(int x, int y, int col) {                    // a music note ♪
    circfill(x, y + 6, 2, col);
    rectfill(x + 2, y, 2, 7, col);
    line(x + 3, y, x + 6, y - 2, col); line(x + 3, y + 1, x + 6, y - 1, col);
}
static void sparkle(int x, int y, int col, float ph) {
    int s = (int)(1.5f + 1.5f * sin_deg(ph));                // twinkle
    line(x - s, y, x + s, y, col); line(x, y - s, x, y + s, col);
    if (s > 2) pset(x, y, CLR_WHITE);
}
// a chunky candy pad (rounded, top-left candy bevel), squished when pressed
static void candy_pad(int x, int y, int w, int h, int face, int hi, int lo, int pressed) {
    blend(BLEND_AVG); rrectfill(x + 1, y + 3, w, h, 4, CLR_BLACK); blend_reset();   // drop shadow
    int dy = pressed ? 2 : 0, hh = pressed ? h - 2 : h;
    rrectfill(x, y + dy, w, hh, 4, face);
    line(x + 3, y + dy + 1, x + w - 4, y + dy + 1, hi);       // top sheen
    line(x + 1, y + dy + 3, x + 1, y + dy + hh - 4, hi);      // left sheen
    blend(BLEND_AVG);
    line(x + 3, y + dy + hh - 1, x + w - 4, y + dy + hh - 1, lo);
    line(x + w - 2, y + dy + 3, x + w - 2, y + dy + hh - 4, lo); blend_reset();
    rrect(x, y + dy, w, hh, 4, CLR_BROWNISH_BLACK);
}
// a chunky purple knob (the icon knob), pointer at angle
static void chunky_knob(int cx, int cy, int r, float v) {
    blend(BLEND_AVG); circfill(cx + 1, cy + 3, r, CLR_BLACK); blend_reset();
    circfill(cx, cy, r, CLR_INDIGO);
    ring(cx, cy, r - 3, r - 1, 165, 285, CLR_PINK);          // top-left glint (PINK > INDIGO in luma)
    ring(cx, cy, r - 3, r - 1, -15, 105, CLR_DARKER_PURPLE); // bottom-right shade
    circ(cx, cy, r, CLR_BROWNISH_BLACK);
    // fluted grip ticks
    for (int a = 0; a < 360; a += 30) line(cx + (int)dx(r - 4, a), cy + (int)dy(r - 4, a), cx + (int)dx(r - 1, a), cy + (int)dy(r - 1, a), CLR_DARKER_PURPLE);
    float ang = 150 + v * 240;
    line(cx + (int)dx(r * 0.4f, ang), cy + (int)dy(r * 0.4f, ang), cx + (int)dx(r - 3, ang), cy + (int)dy(r - 3, ang), CLR_WHITE);
}
// the SLIME MASCOT — the soul on the screen; bops to the beat, blinks
static void mascot(int cx, int cy, float t) {
    int bop = (int)(sin_deg(t * 260) * 2);                   // bounce to an implied beat
    cy += bop;
    int squash = bop < 0 ? 1 : 0;                            // squash at the bottom of the bop
    ovalfill(cx, cy, 14, 12 - squash, CLR_GREEN);            // body
    ovalfill(cx, cy + 3, 13, 9 - squash, CLR_LIME_GREEN);    // lit belly
    int blink = ((int)(t * 1.3f)) % 4 == 0 && sin_deg(t * 300) > 0.7f;
    for (int e = -1; e <= 1; e += 2) {                       // eyes
        int ex = cx + e * 5, ey = cy - 3;
        if (blink) { line(ex - 2, ey, ex + 2, ey, CLR_BROWNISH_BLACK); }
        else { circfill(ex, ey, 3, CLR_WHITE); circfill(ex + 1, ey, 1, CLR_BROWNISH_BLACK); }
    }
    arc(cx, cy + 3, 4, 20, 160, CLR_DARK_GREEN);             // smile
    circfill(cx - 8, cy + 1, 2, CLR_PINK);                   // cheek blush
    circfill(cx + 8, cy + 1, 2, CLR_PINK);
    mnote(cx + 16, cy - 10 - bop, CLR_LIGHT_PEACH);           // a note it hums
}

void draw(void) {
    float t = now();
    // ── the case: a purple shell around a warm cream chassis ──
    cls(CLR_BROWNISH_BLACK);
    rrectfill(4, 4, 312, 392, 16, CLR_DARK_PURPLE);          // shell shadow edge
    rrectfill(4, 4, 308, 388, 16, CLR_INDIGO);               // purple shell
    rrectfill(14, 14, 292, 368, 12, CLR_LIGHT_PEACH);        // cream chassis
    blend(BLEND_AVG); line(20, 16, 300, 16, CLR_WHITE); blend_reset();  // chassis top sheen

    // ── title: handwritten, candy, outlined ──
    font(FONT_COMIC);
    const char *tl = "Tiny", *tr = "Jam";
    for (int o = 0; o < 4; o++) { int ox = (o & 1) ? 1 : -1, oy = (o & 2) ? 1 : -1;   // black outline
        print(tl, 28 + ox, 26 + oy, CLR_BROWNISH_BLACK); print(tr, 150 + ox, 26 + oy, CLR_BROWNISH_BLACK); }
    print(tl, 28, 26, CLR_RED); print(tr, 150, 26, CLR_YELLOW);
    font(FONT_SMALL);
    heart(155, 60, 5, CLR_RED);
    sparkle(120, 30, CLR_LIGHT_YELLOW, t * 200);
    sparkle(232, 34, CLR_WHITE, t * 170 + 90);
    sparkle(250, 60, CLR_LIGHT_YELLOW, t * 230 + 40);

    // ── the LCD screen with the mascot (the SOUL) ──
    rrectfill(24, 70, 168, 96, 6, CLR_BROWNISH_BLACK);       // dark bezel
    rrectfill(28, 74, 160, 88, 4, CLR_DARK_GREEN);           // phosphor field
    blend(BLEND_AVG); for (int y = 76; y < 162; y += 2) line(28, y, 187, y, CLR_BROWNISH_BLACK); blend_reset();  // scanlines
    mascot(90, 118, t);
    font(FONT_SMALL); print("SLIME  #1", 34, 152, CLR_MEDIUM_GREEN);

    // ── speaker grille (icon detail) ──
    for (int i = 0; i < 5; i++) rectfill(206 + i * 6, 74, 3, 22, CLR_DARK_BROWN);

    // ── two chunky purple knobs + a fader with an amber LED ──
    chunky_knob(224, 118, 16, 0.35f);
    chunky_knob(270, 118, 12, 0.7f);
    font(FONT_SMALL); print("CUT", 214, 138, CLR_DARK_BROWN); print("RES", 262, 138, CLR_DARK_BROWN);
    int fx = 296; rrectfill(fx - 2, 74, 5, 70, 2, CLR_BROWNISH_BLACK);   // fader groove
    rrectfill(fx - 6, 100, 12, 8, 2, CLR_MEDIUM_GREY);                   // cap
    line(fx - 5, 104, fx + 4, 104, CLR_DARKER_GREY);
    circfill(fx, 68, 2, CLR_ORANGE); pset(fx - 1, 67, CLR_WHITE);        // amber LED

    // ── the candy pad row ──
    int pc[5] = { CLR_PINK, CLR_ORANGE, CLR_YELLOW, CLR_GREEN, CLR_TRUE_BLUE };
    int ph[5] = { CLR_LIGHT_PEACH, CLR_LIGHT_YELLOW, CLR_LIGHT_YELLOW, CLR_LIME_GREEN, CLR_BLUE };
    int pl[5] = { CLR_DARK_PURPLE, CLR_DARK_ORANGE, CLR_DARK_ORANGE, CLR_DARK_GREEN, CLR_DARK_BLUE };
    int hitpad = ((int)(t * 4)) % 5;                          // one pad "hit" cycles (juice)
    for (int i = 0; i < 5; i++)
        candy_pad(24 + i * 55, 190, 48, 40, pc[i], ph[i], pl[i], i == hitpad);
    // sparkle burst on the hit pad
    int hx = 24 + hitpad * 55 + 24;
    sparkle(hx, 186, CLR_WHITE, t * 400);

    // ── a second pad row + a big round play pad (icon's blue) ──
    for (int i = 0; i < 5; i++)
        candy_pad(24 + i * 55, 236, 48, 40, pc[(i + 2) % 5], ph[(i + 2) % 5], pl[(i + 2) % 5], 0);

    // big round PLAY pad
    blend(BLEND_AVG); circfill(160 + 1, 320 + 3, 34, CLR_BLACK); blend_reset();
    circfill(160, 320, 34, CLR_TRUE_BLUE);
    ring(160, 320, 30, 33, 165, 285, CLR_BLUE);
    ring(160, 320, 30, 33, -15, 105, CLR_DARK_BLUE);
    circ(160, 320, 34, CLR_BROWNISH_BLACK);
    trifill(150, 306, 150, 334, 176, 320, CLR_WHITE);        // play triangle
    mnote(60, 315, CLR_INDIGO); mnote(250, 315, CLR_INDIGO);

    font(FONT_NORMAL);
}
