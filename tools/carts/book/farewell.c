// Chapter-15 mood creature: the farewell — the greeter from Chapter 1, waving you off.
// A bookend: same little blob, same raised hand, a heart instead of a question.
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
    line(cx + 30, cy, cx + 46, cy - lift, CLR_GREEN);   // waving arm
    circfill(cx + 46, cy - lift, 6, CLR_GREEN);
}

void draw(void) {
    cls(CLR_DARK_BLUE);
    int cx = SCREEN_W / 2, cy = SCREEN_H / 2 + 6;
    // a little heart floating up where the greeter's sparkles were
    int hx = cx - 66, hy = cy - 44;
    circfill(hx - 3, hy, 3, CLR_RED);
    circfill(hx + 3, hy, 3, CLR_RED);
    trifill(hx - 5, hy + 1, hx + 5, hy + 1, hx, hy + 7, CLR_RED);
    circfill(hx - 12, hy + 14, 2, CLR_RED);             // a smaller one drifting
    blob(cx, cy, 26);
}
