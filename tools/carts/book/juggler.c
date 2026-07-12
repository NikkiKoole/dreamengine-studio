// Chapter-3 mood creature: the juggler — a blob with the shape kit orbiting it.
#include "studio.h"

static void blob(int cx, int cy) {
    ovalfill(cx, cy + 40, 40, 9, CLR_BLACK);
    ovalfill(cx, cy, 34, 40, CLR_GREEN);
    ovalfill(cx, cy + 10, 20, 22, CLR_YELLOW);
    circfill(cx - 12, cy - 16, 9, CLR_WHITE);
    circfill(cx + 12, cy - 16, 9, CLR_WHITE);
    circfill(cx - 10, cy - 18, 4, CLR_BLACK);            // pupils up — watching the shapes
    circfill(cx + 14, cy - 18, 4, CLR_BLACK);
    arc(cx, cy - 2, 13, 15, 165, CLR_BLACK);
    line(cx - 8, cy - 28, cx - 14, cy - 40, CLR_GREEN);
    line(cx + 8, cy - 28, cx + 14, cy - 40, CLR_GREEN);
    circfill(cx - 14, cy - 41, 3, CLR_RED);
    circfill(cx + 14, cy - 41, 3, CLR_RED);
    ovalfill(cx - 14, cy + 39, 10, 5, CLR_DARK_GREEN);
    ovalfill(cx + 14, cy + 39, 10, 5, CLR_DARK_GREEN);
    // raised arms
    line(cx - 30, cy, cx - 44, cy - 20, CLR_GREEN);
    line(cx + 30, cy, cx + 44, cy - 20, CLR_GREEN);
}

void draw(void) {
    cls(CLR_DARK_BLUE);
    int cx = SCREEN_W / 2, cy = SCREEN_H / 2 + 8;
    // the shape kit, mid-air
    trifill(cx - 66, cy - 40, cx - 50, cy - 40, cx - 58, cy - 58, CLR_RED);   // triangle
    rect(cx + 48, cy - 56, 18, 18, CLR_BLUE);                                 // square
    rectfill(cx + 51, cy - 53, 12, 12, CLR_BLUE);
    circfill(cx, cy - 74, 9, CLR_ORANGE);                                     // circle
    blob(cx, cy);
}
