/* de:meta
{
  "title": "22. fill patterns",
  "status": "active",
  "kind": [
    "tutorial"
  ],
  "teaches": [],
  "description": "PICO-8-style fillp: rectfill_pat() tiles a 4x4 bitmask across a rect so two colors make a texture. Shows a dither GRADIENT background, the named patterns (FILL_CHECKER/DOTS/HLINES/VLINES/DIAG/GRID), and a panel that re-rolls a RANDOM 16-bit pattern every second (any value is valid). A: change the color pair."
}
de:meta */
#include "studio.h"

// FILL PATTERNS — PICO-8 fillp. Set a 4×4 pattern + a hole color, then your
// NORMAL fills draw it: the draw color fills the 0-bits, the hole color the
// 1-bits (pass -1 for transparent). fillp_reset() goes back to solid. Works on
// rectfill, circfill and trifill alike. Stack densities → a dither GRADIENT.
//
// A: change the color pair

static const int DENS[6]   = { 0xFFFF, 0xFAFA, 0xA5A5, 0xA0A0, 0x8020, 0x0000 }; // top=a … bottom=b
static const int PATS[6]   = { FILL_CHECKER, FILL_DOTS, FILL_HLINES, FILL_VLINES, FILL_DIAG, FILL_GRID };
static const char *NAME[6] = { "CHECK", "DOTS", "HLINE", "VLINE", "DIAG", "GRID" };
static int pair, randpat = 0xA5A5, lastSec = -1;

void update(void) {
    if (btnp(0, BTN_A) || btnp(1, BTN_A)) pair = (pair + 1) % 4;
    int s = (int)now();
    if (s != lastSec) { lastSec = s; randpat = rnd(0x10000); }
}

void draw(void) {
    int lo[4] = { CLR_DARKER_BLUE, CLR_DARK_PURPLE, CLR_DARK_GREEN, CLR_BLACK };
    int hi[4] = { CLR_BLUE,        CLR_PINK,        CLR_LIME_GREEN, CLR_ORANGE };
    int a = lo[pair], b = hi[pair];

    cls(CLR_BLACK);

    // dither gradient — same b/a, just denser b toward the bottom
    for (int i = 0; i < 6; i++) { fillp(DENS[i], a); rectfill(0, i * 34, SCREEN_W, 34, b); }
    fillp_reset();

    const char *t = "FILL PATTERNS";
    int tx = (SCREEN_W - text_width(t) * 2) / 2;
    print_scaled(t, tx + 2, 10, CLR_DARK_BLUE, 2);
    print_scaled(t, tx, 8, CLR_WHITE, 2);

    // a RANDOM pattern, re-rolled every second — any 16-bit value is valid
    int bx = 70, by = 42, bw = 180, bh = 52;
    print_centered("random pattern, new every second:", SCREEN_W/2, by - 9, CLR_LIGHT_GREY);
    fillp(randpat, a); rectfill(bx, by, bw, bh, b); fillp_reset();
    rect(bx, by, bw, bh, CLR_WHITE);
    print_centered(str("0x%04X", randpat & 0xFFFF), SCREEN_W/2, by + bh + 3, CLR_YELLOW);

    // the SAME global pattern works on circles & triangles, not just rects
    fillp(FILL_DIAG, a); circfill(32, 66, 21, b); fillp_reset(); circ(32, 66, 21, CLR_WHITE);
    fillp(FILL_GRID, a); trifill(288, 44, 264, 92, 312, 92, b); fillp_reset();

    // a swatch of each named pattern
    for (int i = 0; i < 6; i++) {
        int x = 6 + i * 51, y = 130;
        fillp(PATS[i], a); rectfill(x, y, 46, 28, b); fillp_reset();
        rect(x, y, 46, 28, CLR_WHITE);
        print(NAME[i], x + 4, y + 30, CLR_LIGHT_GREY);
    }

    print_centered("A: change colors", SCREEN_W/2, SCREEN_H - 11, CLR_DARK_BLUE);
    print_centered("A: change colors", SCREEN_W/2, SCREEN_H - 12, CLR_WHITE);
}
