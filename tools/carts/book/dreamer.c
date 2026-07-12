// Chapter-4 mood creature: the dreamer — a blob swaying, eyes shut, riding its own wave.
#include "studio.h"

void draw(void) {
    cls(CLR_DARK_BLUE);
    int cx = SCREEN_W / 2, cy = SCREEN_H / 2 + 8;
    // a moon
    circfill(cx + 92, cy - 44, 16, CLR_YELLOW);
    circfill(cx + 84, cy - 48, 14, CLR_DARK_BLUE);
    // the sine wave it rides
    for (int x = 0; x < SCREEN_W; x++) {
        int y = cy + 44 + (int)(sin_deg(x * 3.0f) * 8);
        pset(x, y, CLR_DARK_GREEN);
    }
    ovalfill(cx, cy + 40, 40, 9, CLR_BLACK);
    ovalfill(cx, cy, 34, 40, CLR_GREEN);
    ovalfill(cx, cy + 10, 20, 22, CLR_YELLOW);
    // sleepy shut eyes (downward arcs)
    arc(cx - 12, cy - 12, 7, 200, 340, CLR_BLACK);
    arc(cx + 12, cy - 12, 7, 200, 340, CLR_BLACK);
    arc(cx, cy - 2, 10, 20, 160, CLR_BLACK);            // content little smile
    line(cx - 8, cy - 28, cx - 14, cy - 40, CLR_GREEN);
    line(cx + 8, cy - 28, cx + 14, cy - 40, CLR_GREEN);
    circfill(cx - 14, cy - 41, 3, CLR_RED);
    circfill(cx + 14, cy - 41, 3, CLR_RED);
    ovalfill(cx - 14, cy + 39, 10, 5, CLR_DARK_GREEN);
    ovalfill(cx + 14, cy + 39, 10, 5, CLR_DARK_GREEN);
    // floating z's
    print("z", cx - 44, cy - 30, CLR_WHITE);
    print("z", cx - 54, cy - 46, CLR_LIGHT_GREY);
}
