// Chapter-11 mood creature: the scout — a blob with binoculars, spying the far world.
#include "studio.h"

void draw(void) {
    cls(CLR_DARK_BLUE);
    int cx = SCREEN_W / 2 - 20, cy = SCREEN_H / 2 + 8;
    // distant world beyond, tiny (what it's looking at)
    line(cx + 70, 150, SCREEN_W, 150, CLR_DARK_GREEN);
    for (int i = 0; i < 4; i++) circfill(cx + 92 + i * 26, 148, 5 + (i % 2) * 3, CLR_DARK_GREEN);
    circfill(SCREEN_W - 24, 40, 10, CLR_YELLOW);        // a far sun
    // the blob
    ovalfill(cx, cy + 40, 40, 9, CLR_BLACK);
    ovalfill(cx, cy, 34, 40, CLR_GREEN);
    ovalfill(cx, cy + 10, 20, 22, CLR_YELLOW);
    arc(cx, cy - 2, 12, 20, 160, CLR_BLACK);            // smile under the binoculars
    line(cx - 8, cy - 28, cx - 14, cy - 40, CLR_GREEN);
    line(cx + 8, cy - 28, cx + 14, cy - 40, CLR_GREEN);
    circfill(cx - 14, cy - 41, 3, CLR_RED);
    circfill(cx + 14, cy - 41, 3, CLR_RED);
    ovalfill(cx - 14, cy + 39, 10, 5, CLR_DARK_GREEN);
    ovalfill(cx + 14, cy + 39, 10, 5, CLR_DARK_GREEN);
    // binoculars held up to the eyes, pointing right
    rectfill(cx - 16, cy - 20, 12, 10, CLR_DARK_GREY);
    rectfill(cx + 4, cy - 20, 12, 10, CLR_DARK_GREY);
    rectfill(cx - 4, cy - 18, 8, 5, CLR_DARK_GREY);     // bridge
    circfill(cx - 4, cy - 15, 3, CLR_BLUE);             // lenses glint, aimed at the world
    circfill(cx + 16, cy - 15, 3, CLR_BLUE);
    // arms up to hold them
    line(cx - 24, cy + 2, cx - 12, cy - 12, CLR_GREEN);
    line(cx + 24, cy + 2, cx + 12, cy - 12, CLR_GREEN);
}
