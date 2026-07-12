// Chapter-8 mood creature: the bumper — a blob walking smack into a wall, POW.
#include "studio.h"

void draw(void) {
    cls(CLR_DARK_BLUE);
    int cx = SCREEN_W / 2 - 18, cy = SCREEN_H / 2 + 8;
    // the wall it hit
    rectfill(cx + 40, cy - 46, 22, 96, CLR_DARK_GREY);
    rect(cx + 40, cy - 46, 22, 96, CLR_LIGHT_GREY);
    // impact star at the contact point
    int ix = cx + 38, iy = cy - 8;
    for (int a = 0; a < 360; a += 45) {
        int r = (a % 90 == 0) ? 15 : 8;
        line(ix, iy, ix + (int)(cos_deg(a) * r), iy + (int)(sin_deg(a) * r), CLR_YELLOW);
    }
    circfill(ix, iy, 5, CLR_ORANGE);
    // squished blob (wider, leaning into the wall)
    ovalfill(cx, cy + 40, 40, 9, CLR_BLACK);
    ovalfill(cx + 4, cy, 32, 40, CLR_GREEN);
    ovalfill(cx + 2, cy + 10, 20, 22, CLR_YELLOW);
    circfill(cx - 4, cy - 16, 8, CLR_WHITE);
    circfill(cx + 12, cy - 16, 8, CLR_WHITE);
    // dizzy spiral-ish eyes (x marks)
    line(cx - 7, cy - 19, cx - 1, cy - 13, CLR_BLACK);
    line(cx - 1, cy - 19, cx - 7, cy - 13, CLR_BLACK);
    line(cx + 9, cy - 19, cx + 15, cy - 13, CLR_BLACK);
    line(cx + 15, cy - 19, cx + 9, cy - 13, CLR_BLACK);
    line(cx - 2, cy + 2, cx + 8, cy + 2, CLR_BLACK);    // flat dazed mouth
    line(cx - 4, cy - 28, cx - 10, cy - 40, CLR_GREEN); // antennae rattled
    line(cx + 8, cy - 28, cx + 16, cy - 42, CLR_GREEN);
    circfill(cx - 10, cy - 41, 3, CLR_RED);
    circfill(cx + 16, cy - 43, 3, CLR_RED);
    ovalfill(cx - 12, cy + 39, 10, 5, CLR_DARK_GREEN);
    ovalfill(cx + 14, cy + 39, 10, 5, CLR_DARK_GREEN);
}
