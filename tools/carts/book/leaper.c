// Chapter-2 mood creature: the leaper — a blob jumping the gap at the world's edge.
#include "studio.h"

static void face(int cx, int cy) {
    ovalfill(cx, cy, 34, 40, CLR_GREEN);
    ovalfill(cx, cy + 10, 20, 22, CLR_YELLOW);
    circfill(cx - 12, cy - 16, 9, CLR_WHITE);
    circfill(cx + 12, cy - 16, 9, CLR_WHITE);
    circfill(cx - 8, cy - 18, 4, CLR_BLACK);            // pupils up — looking where it leaps
    circfill(cx + 16, cy - 18, 4, CLR_BLACK);
    arc(cx, cy - 2, 12, 20, 160, CLR_BLACK);
    line(cx - 8, cy - 28, cx - 16, cy - 42, CLR_GREEN); // antennae swept back by speed
    line(cx + 8, cy - 28, cx + 0,  cy - 44, CLR_GREEN);
    circfill(cx - 16, cy - 43, 3, CLR_RED);
    circfill(cx + 0,  cy - 45, 3, CLR_RED);
    ovalfill(cx - 12, cy + 40, 9, 5, CLR_DARK_GREEN);   // tucked feet
    ovalfill(cx + 12, cy + 40, 9, 5, CLR_DARK_GREEN);
}

void draw(void) {
    cls(CLR_DARK_BLUE);
    int gy = SCREEN_H - 26;
    rectfill(0, gy, SCREEN_W / 2 - 10, 40, CLR_DARK_GREEN);   // ledge on the left
    rectfill(SCREEN_W / 2 + 40, gy, SCREEN_W, 40, CLR_DARK_GREEN); // far ledge on the right
    for (int y = gy - 6; y < SCREEN_H; y += 8)                // the red edge, from ch.2
        pset(SCREEN_W / 2 - 10, y, CLR_RED);
    int cx = SCREEN_W / 2 + 6, cy = gy - 44;
    for (int i = 1; i <= 4; i++)                              // motion streaks behind
        line(cx - 30 - i * 10, cy + 4 + i * 3, cx - 18 - i * 10, cy + 4 + i * 3, CLR_LIGHT_GREY);
    face(cx, cy);
}
