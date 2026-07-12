// Chapter-9 mood creature: the crowd — one blob, and a whole lot of little ones.
#include "studio.h"

static void mini(int cx, int cy) {                 // a tiny member of the cast
    ovalfill(cx, cy, 8, 10, CLR_GREEN);
    ovalfill(cx, cy + 3, 4, 5, CLR_YELLOW);
    circfill(cx - 3, cy - 4, 2, CLR_WHITE);
    circfill(cx + 3, cy - 4, 2, CLR_WHITE);
    pset(cx - 3, cy - 4, CLR_BLACK); pset(cx + 3, cy - 4, CLR_BLACK);
}

void draw(void) {
    cls(CLR_DARK_BLUE);
    int cx = SCREEN_W / 2, cy = SCREEN_H / 2 + 6;
    // the little ones, scattered around
    int sx[] = { -74, 66, -52, 88, 44, -90, 20 };
    int sy[] = { 28, -18, -34, 30, 40, -6, -44 };
    for (int i = 0; i < 7; i++) mini(cx + sx[i], cy + sy[i]);
    // the big one
    ovalfill(cx, cy + 40, 40, 9, CLR_BLACK);
    ovalfill(cx, cy, 34, 40, CLR_GREEN);
    ovalfill(cx, cy + 10, 20, 22, CLR_YELLOW);
    circfill(cx - 12, cy - 16, 9, CLR_WHITE);
    circfill(cx + 12, cy - 16, 9, CLR_WHITE);
    circfill(cx - 11, cy - 16, 4, CLR_BLACK);
    circfill(cx + 13, cy - 16, 4, CLR_BLACK);
    arc(cx, cy - 2, 12, 20, 160, CLR_BLACK);
    line(cx - 8, cy - 28, cx - 14, cy - 40, CLR_GREEN);
    line(cx + 8, cy - 28, cx + 14, cy - 40, CLR_GREEN);
    circfill(cx - 14, cy - 41, 3, CLR_RED);
    circfill(cx + 14, cy - 41, 3, CLR_RED);
    ovalfill(cx - 14, cy + 39, 10, 5, CLR_DARK_GREEN);
    ovalfill(cx + 14, cy + 39, 10, 5, CLR_DARK_GREEN);
}
