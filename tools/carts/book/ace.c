// Chapter-13 mood creature: the ace — a blob piloting its own little rocket.
#include "studio.h"

void draw(void) {
    cls(CLR_DARK_BLUE);
    int cx = SCREEN_W / 2, cy = SCREEN_H / 2 - 4;
    // stars
    int sx[] = { -78, 80, -60, 92, -96, 64 };
    int sy[] = { -34, -40, 30, 18, -8, 40 };
    for (int i = 0; i < 6; i++) { pset(cx + sx[i], cy + sy[i], CLR_WHITE);
        pset(cx + sx[i] + 1, cy + sy[i], CLR_LIGHT_GREY); }
    // flame
    trifill(cx - 10, cy + 44, cx + 10, cy + 44, cx, cy + 70, CLR_ORANGE);
    trifill(cx - 6, cy + 44, cx + 6, cy + 44, cx, cy + 60, CLR_YELLOW);
    // rocket body
    rectfill(cx - 18, cy - 20, 36, 64, CLR_LIGHT_GREY);
    trifill(cx - 18, cy - 20, cx + 18, cy - 20, cx, cy - 52, CLR_RED);   // nose cone
    trifill(cx - 18, cy + 24, cx - 34, cy + 48, cx - 18, cy + 44, CLR_RED);  // fins
    trifill(cx + 18, cy + 24, cx + 34, cy + 48, cx + 18, cy + 44, CLR_RED);
    // cockpit window with the pilot
    circfill(cx, cy + 2, 14, CLR_DARK_BLUE);
    circfill(cx, cy + 4, 11, CLR_GREEN);                 // blob head
    circfill(cx - 4, cy, 3, CLR_WHITE); circfill(cx + 4, cy, 3, CLR_WHITE);
    pset(cx - 4, cy, CLR_BLACK); pset(cx + 4, cy, CLR_BLACK);
    arc(cx, cy + 4, 6, 20, 160, CLR_BLACK);              // grin
    circ(cx, cy + 2, 14, CLR_DARK_GREY);                 // window rim
}
