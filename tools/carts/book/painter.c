// Chapter-6 mood creature: the painter — a blob dabbing a tiny pixel picture into being.
#include "studio.h"

void draw(void) {
    cls(CLR_DARK_BLUE);
    int cx = SCREEN_W / 2 - 16, cy = SCREEN_H / 2 + 10;
    // a little 4x4 pixel sprite floating beside it, being painted
    int px = cx + 54, py = cy - 34, s = 9;
    int art[16] = { 0,10,10,0,  10,7,7,10,  10,7,7,10,  0,10,10,0 };
    rectfill(px - 3, py - 3, s * 4 + 6, s * 4 + 6, CLR_DARK_GREY);   // canvas
    for (int i = 0; i < 16; i++)
        if (art[i]) rectfill(px + (i % 4) * s, py + (i / 4) * s, s, s, art[i]);
    // the blob
    ovalfill(cx, cy + 40, 40, 9, CLR_BLACK);
    ovalfill(cx, cy, 34, 40, CLR_GREEN);
    ovalfill(cx, cy + 10, 20, 22, CLR_YELLOW);
    circfill(cx - 12, cy - 16, 9, CLR_WHITE);
    circfill(cx + 12, cy - 16, 9, CLR_WHITE);
    circfill(cx - 9, cy - 16, 4, CLR_BLACK);            // looking at its work
    circfill(cx + 15, cy - 16, 4, CLR_BLACK);
    arc(cx, cy - 2, 12, 20, 160, CLR_BLACK);
    line(cx - 8, cy - 28, cx - 14, cy - 40, CLR_GREEN);
    line(cx + 8, cy - 28, cx + 14, cy - 40, CLR_GREEN);
    circfill(cx - 14, cy - 41, 3, CLR_RED);
    circfill(cx + 14, cy - 41, 3, CLR_RED);
    ovalfill(cx - 14, cy + 39, 10, 5, CLR_DARK_GREEN);
    ovalfill(cx + 14, cy + 39, 10, 5, CLR_DARK_GREEN);
    // arm holding a paintbrush toward the canvas
    line(cx + 28, cy - 2, px - 8, py + 18, CLR_GREEN);
    line(px - 8, py + 18, px - 1, py + 14, CLR_BROWN);  // brush handle
    circfill(px - 1, py + 13, 3, CLR_RED);              // loaded bristle
}
