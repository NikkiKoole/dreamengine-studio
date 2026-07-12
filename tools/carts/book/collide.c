// Chapter-8 illustration: two boxes. One sweeps through the other; when they overlap
// both light up red and the shared patch is shown in yellow — that's all "collision" is.
#include "studio.h"

void draw(void) {
    cls(CLR_DARK_BLUE);

    // box B — a fixed wall
    int bx = 130, by = 74, bw = 64, bh = 64;
    // box A — sweeps left and right (loops every 120 frames)
    int aw = 44, ah = 44;
    int ax = SCREEN_W / 2 - aw / 2 + (int)(sin_deg(frame() * 3) * 118);
    int ay = 84;

    // the one true/false question: do the two rectangles overlap?
    int hit = ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;

    rectfill(bx, by, bw, bh, hit ? CLR_RED : CLR_DARK_GREY);
    rectfill(ax, ay, aw, ah, hit ? CLR_RED : CLR_BLUE);

    if (hit) {
        // the shared patch = the overlap rectangle, drawn in yellow
        int ox = ax > bx ? ax : bx;
        int oy = ay > by ? ay : by;
        int ox2 = (ax + aw < bx + bw) ? ax + aw : bx + bw;
        int oy2 = (ay + ah < by + bh) ? ay + ah : by + bh;
        rectfill(ox, oy, ox2 - ox, oy2 - oy, CLR_YELLOW);
        print_centered("TOUCHING!", SCREEN_W / 2, 24, CLR_YELLOW);
    } else {
        print_centered("clear", SCREEN_W / 2, 24, CLR_LIGHT_GREY);
    }
}
