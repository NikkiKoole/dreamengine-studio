/* de:meta
{
  "title": "23. holes & colors",
  "status": "active",
  "kind": [
    "tutorial"
  ],
  "teaches": [],
  "description": "The two sides of fillp(): pass -1 as the hole color and the 1-bits are TRANSPARENT (the scene shows through); pass a palette color and you get an opaque two-tone fill. Same pattern, side by side, over a rainbow so the difference is obvious. Works on rects and circles. A: change pattern."
}
de:meta */
#include "studio.h"

// FILLP — HOLES vs COLORS. The SAME pattern, drawn two ways, over a rainbow
// background so you can see what the 1-bits do:
//   LEFT   fillp(pat, -1)            → 1-bits are TRANSPARENT (the scene shows through)
//   RIGHT  fillp(pat, CLR_DARK_BLUE) → 1-bits are an opaque color (scene hidden)
// Works the same on rects and circles. A: change the pattern.

static const int   PATS[6] = { FILL_CHECKER, FILL_DOTS, FILL_DIAG, FILL_HLINES, FILL_VLINES, FILL_GRID };
static const char *NAME[6] = { "CHECKER", "DOTS", "DIAG", "HLINES", "VLINES", "GRID" };
static int p;

void update(void) {
    if (btnp(0, BTN_A) || btnp(1, BTN_A)) p = (p + 1) % 6;
}

void draw(void) {
    cls(CLR_BLACK);

    // busy rainbow background so the transparent holes are obvious
    int cols[8] = { CLR_RED, CLR_ORANGE, CLR_YELLOW, CLR_GREEN,
                    CLR_BLUE, CLR_INDIGO, CLR_PINK, CLR_LIME_GREEN };
    for (int i = 0; i < 8; i++) rectfill(i * 40, 26, 40, 152, cols[i]);

    int pat = PATS[p];

    // LEFT — transparent holes: the rainbow shows through the 1-bits
    fillp(pat, -1);
    rectfill(18, 42, 124, 78, CLR_WHITE);
    circfill(80, 150, 20, CLR_WHITE);
    fillp_reset();
    rect(18, 42, 124, 78, CLR_BLACK);

    // RIGHT — opaque two-tone: 0-bits white, 1-bits dark blue, rainbow hidden
    fillp(pat, CLR_DARK_BLUE);
    rectfill(178, 42, 124, 78, CLR_WHITE);
    circfill(240, 150, 20, CLR_WHITE);
    fillp_reset();
    rect(178, 42, 124, 78, CLR_BLACK);

    // top + bottom HUD bars (so the labels stay readable over the rainbow)
    rectfill(0, 0, SCREEN_W, 16, CLR_BLACK);
    print_centered(str("FILLP   -   %s", NAME[p]), SCREEN_W/2, 4, CLR_WHITE);

    rectfill(0, SCREEN_H - 26, SCREEN_W, 26, CLR_BLACK);
    print("holes:  fillp(p, -1)", 14, SCREEN_H - 22, CLR_LIGHT_PEACH);
    print_right("2 colors: fillp(p, c)", SCREEN_W - 14, SCREEN_H - 22, CLR_LIGHT_PEACH);
    print_centered("A: change pattern", SCREEN_W/2, SCREEN_H - 12, CLR_DARK_GREY);
}
