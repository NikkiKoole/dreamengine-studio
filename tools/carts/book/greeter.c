// Chapter-1 mood creature: the greeter — a little blob waving hello.
#include "studio.h"

static void blob(int cx, int cy, int lift) {
    ovalfill(cx, cy + 40, 40, 9, CLR_BLACK);            // shadow
    ovalfill(cx, cy, 34, 40, CLR_GREEN);                // body
    ovalfill(cx, cy + 10, 20, 22, CLR_YELLOW);          // belly
    circfill(cx - 12, cy - 16, 9, CLR_WHITE);
    circfill(cx + 12, cy - 16, 9, CLR_WHITE);
    circfill(cx - 10, cy - 14, 4, CLR_BLACK);
    circfill(cx + 14, cy - 14, 4, CLR_BLACK);
    arc(cx, cy - 2, 12, 20, 160, CLR_BLACK);            // smile
    line(cx - 8, cy - 28, cx - 14, cy - 40, CLR_GREEN); // antennae
    line(cx + 8, cy - 28, cx + 14, cy - 40, CLR_GREEN);
    circfill(cx - 14, cy - 41, 3, CLR_RED);
    circfill(cx + 14, cy - 41, 3, CLR_RED);
    ovalfill(cx - 14, cy + 39, 10, 5, CLR_DARK_GREEN);  // feet
    ovalfill(cx + 14, cy + 39, 10, 5, CLR_DARK_GREEN);
    // a waving arm on the right, lifted
    line(cx + 30, cy, cx + 46, cy - lift, CLR_GREEN);
    circfill(cx + 46, cy - lift, 6, CLR_GREEN);
}

void draw(void) {
    cls(CLR_DARK_BLUE);
    int cx = SCREEN_W / 2, cy = SCREEN_H / 2 + 6;
    // little sparkles
    for (int i = 0; i < 3; i++) {
        int sx = cx - 70 + i * 12, sy = cy - 48 - i * 6;
        line(sx - 3, sy, sx + 3, sy, CLR_YELLOW);
        line(sx, sy - 3, sx, sy + 3, CLR_YELLOW);
    }
    blob(cx, cy, 26);
}
