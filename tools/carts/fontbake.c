/* de:meta
{
  "slug": "fontbake",
  "title": "font bake",
  "status": "active",
  "created": "2026-06-05",
  "kind": [
    "tech-demo"
  ],
  "teaches": [],
  "description": "Title text baked from a real TTF (Google Font \"Bungee\") at BUILD time — no font code in the cart at all. tools/font-bake.js parses the TTF with a vendored opentype.js, flattens the glyph outlines and scanline-fills them (3x3 supersampled, optional darker AA edge) straight into sprite-draw canvases; fontbake.cart.js centers each word into a fixed slot-rect so the C side just sspr()s two sheet regions. The title sine-waves in 4px strips at 2x, and because baked text is ordinary pixels, pal() recolors it live. Drop any Google Fonts TTF into tools/fonts/ and bake your own. Z cycles the title color."
}
de:meta */
// fontbake — title screen whose big text was baked from a real TTF
// (Google Font "Bungee") at BUILD time by tools/font-bake.js.
//
// There is no font code in here at all: the rasterized words are ordinary
// sprites in the sheet (see fontbake.cart.js for the bake). The C side just
// sspr()s two fixed sheet regions:
//   (0,  0) 128x32  "DREAM"   slots 0-15
//   (0, 32) 128x16  "ENGINE"  slots 16-23
//
// And because baked text is just pixels, pal() recolors it like anything
// else — press Z to cycle the title color.
#include "studio.h"
#include <math.h>

static int cycle = 0;

void init(void) {
    colorkey(0);    // baked banners: palette 0 around the words = transparent
}                   // (without this each banner drags an opaque black slot-rect)

void update(void) {
    if (btnp(0, BTN_A)) cycle = (cycle + 1) % 4;
}

void draw(void) {
    float t = now();
    int hz = 132;                       // horizon line

    cls(CLR_BLACK);

    // night sky with a band of glow at the horizon
    rectfill(0, hz - 26, SCREEN_W, 18, CLR_DARKER_BLUE);
    rectfill(0, hz - 8, SCREEN_W, 8, CLR_DARK_BLUE);

    // twinkling stars (hash positions so they're stable)
    for (int i = 0; i < 48; i++) {
        int sx = (i * 67 + 13) % SCREEN_W, sy = (i * 41 + 7) % (hz - 30);
        if (((frame() / 16) + i) % 9)
            pset(sx, sy, (i % 4) ? CLR_DARK_GREY : CLR_LIGHT_GREY);
    }

    // scrolling perspective grid floor
    for (int i = 0; i < 14; i++) {
        float ph = fmodf(t * 0.4f + i / 14.0f, 1.0f);
        int gy = hz + (int)(ph * ph * (SCREEN_H - hz));
        line(0, gy, SCREEN_W, gy, CLR_DARK_PURPLE);
    }
    for (int i = -10; i <= 10; i++)
        line(SCREEN_W / 2 + i * 7, hz, SCREEN_W / 2 + i * 52, SCREEN_H, CLR_DARK_PURPLE);
    line(0, hz, SCREEN_W, hz, CLR_PINK);

    // "DREAM" — baked sheet region (0,0,128,32) drawn 2x, sine-waved in strips.
    // recolor BOTH baked colors: the fill and its AA edge — the edge is its own
    // palette index (orange), so swapping the fill alone leaves a clashing rim
    static const int tints[4] = { CLR_YELLOW, CLR_PINK, CLR_LIME_GREEN, CLR_BLUE };
    static const int edges[4] = { CLR_ORANGE, CLR_DARK_RED, CLR_MEDIUM_GREEN, CLR_TRUE_BLUE };
    if (cycle) { pal(CLR_YELLOW, tints[cycle]); pal(CLR_ORANGE, edges[cycle]); }
    for (int sx = 0; sx < 128; sx += 4) {
        int off = (int)(sinf(t * 2.2f + sx * 0.05f) * 3.0f);
        sspr(sx, 0, 4, 32, 32 + sx * 2, 34 + off, 8, 64);
    }
    pal_reset();

    // "ENGINE" — baked sheet region (0,32,128,16), steady
    sspr(0, 32, 128, 16, 96, 104, 128, 16);

    print_centered("BAKED FROM A REAL TTF AT BUILD TIME", SCREEN_W / 2, 146, CLR_LIGHT_GREY);
    print_centered("BUNGEE - GOOGLE FONTS", SCREEN_W / 2, 158, CLR_DARK_GREY);
    if ((frame() / 24) % 2)
        print_centered("PRESS Z TO RECOLOR", SCREEN_W / 2, 180, CLR_WHITE);
}
