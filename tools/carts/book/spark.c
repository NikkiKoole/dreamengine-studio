// Chapter-10 mood creature: the spark — a blob leaping with pure joy, all sparkle.
#include "studio.h"

void draw(void) {
    cls(CLR_DARK_BLUE);
    int cx = SCREEN_W / 2, cy = SCREEN_H / 2 - 4;
    // sparkles all around
    int sx[] = { -70, 70, -84, 88, -40, 52 };
    int sy[] = { -30, -36, 18, 10, -54, -50 };
    for (int i = 0; i < 6; i++) {
        int x = cx + sx[i], y = cy + sy[i], r = (i % 2) ? 7 : 4;
        line(x - r, y, x + r, y, CLR_YELLOW);
        line(x, y - r, x, y + r, CLR_YELLOW);
    }
    // motion lines under it (mid-air)
    for (int i = 0; i < 3; i++) line(cx - 20 + i * 14, cy + 58, cx - 14 + i * 14, cy + 64, CLR_LIGHT_GREY);
    // a stretched, leaping blob (taller than usual = stretch)
    ovalfill(cx, cy + 62, 26, 6, CLR_BLACK);            // shadow, small + far = high jump
    ovalfill(cx, cy, 30, 46, CLR_GREEN);                // stretched body
    ovalfill(cx, cy + 12, 18, 24, CLR_YELLOW);
    circfill(cx - 11, cy - 18, 8, CLR_WHITE);
    circfill(cx + 11, cy - 18, 8, CLR_WHITE);
    circfill(cx - 9, cy - 20, 4, CLR_BLACK);            // eyes up, thrilled
    circfill(cx + 13, cy - 20, 4, CLR_BLACK);
    arc(cx, cy - 4, 14, 15, 165, CLR_BLACK);            // huge grin
    // arms flung up
    line(cx - 26, cy - 4, cx - 40, cy - 30, CLR_GREEN);
    line(cx + 26, cy - 4, cx + 40, cy - 30, CLR_GREEN);
    circfill(cx - 40, cy - 30, 4, CLR_GREEN);
    circfill(cx + 40, cy - 30, 4, CLR_GREEN);
    line(cx - 8, cy - 34, cx - 14, cy - 48, CLR_GREEN); // antennae up
    line(cx + 8, cy - 34, cx + 14, cy - 48, CLR_GREEN);
    circfill(cx - 14, cy - 49, 3, CLR_RED);
    circfill(cx + 14, cy - 49, 3, CLR_RED);
    ovalfill(cx - 12, cy + 44, 8, 5, CLR_DARK_GREEN);   // tucked feet
    ovalfill(cx + 12, cy + 44, 8, 5, CLR_DARK_GREEN);
}
