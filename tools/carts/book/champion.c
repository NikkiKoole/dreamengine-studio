// Chapter-5 mood creature: the champion — a blob holding up its catch, confetti falling.
#include "studio.h"

void draw(void) {
    cls(CLR_DARK_BLUE);
    int cx = SCREEN_W / 2, cy = SCREEN_H / 2 + 12;
    // confetti
    int cc[] = { CLR_RED, CLR_YELLOW, CLR_GREEN, CLR_BLUE, CLR_ORANGE, CLR_WHITE };
    for (int i = 0; i < 14; i++) {
        int x = 20 + (i * 47) % (SCREEN_W - 40);
        int y = 12 + (i * 29) % 70;
        rectfill(x, y, 3, 3, cc[i % 6]);
    }
    ovalfill(cx, cy + 40, 40, 9, CLR_BLACK);
    ovalfill(cx, cy, 34, 40, CLR_GREEN);
    ovalfill(cx, cy + 10, 20, 22, CLR_YELLOW);
    circfill(cx - 12, cy - 16, 9, CLR_WHITE);
    circfill(cx + 12, cy - 16, 9, CLR_WHITE);
    circfill(cx - 11, cy - 16, 4, CLR_BLACK);
    circfill(cx + 13, cy - 16, 4, CLR_BLACK);
    arc(cx, cy - 4, 14, 15, 165, CLR_BLACK);            // big grin
    line(cx - 8, cy - 28, cx - 14, cy - 40, CLR_GREEN);
    line(cx + 8, cy - 28, cx + 14, cy - 40, CLR_GREEN);
    circfill(cx - 14, cy - 41, 3, CLR_RED);
    circfill(cx + 14, cy - 41, 3, CLR_RED);
    ovalfill(cx - 14, cy + 39, 10, 5, CLR_DARK_GREEN);
    ovalfill(cx + 14, cy + 39, 10, 5, CLR_DARK_GREEN);
    // both arms up, holding a basket with a caught cherry
    line(cx - 28, cy - 2, cx - 40, cy - 30, CLR_GREEN);
    line(cx + 28, cy - 2, cx + 40, cy - 30, CLR_GREEN);
    trifill(cx - 44, cy - 30, cx + 44, cy - 30, cx + 30, cy - 12, CLR_BROWN); // basket
    trifill(cx - 44, cy - 30, cx + 30, cy - 12, cx - 30, cy - 12, CLR_BROWN);
    circfill(cx, cy - 38, 7, CLR_RED);                  // the catch
}
