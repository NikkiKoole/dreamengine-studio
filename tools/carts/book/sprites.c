// Chapter-6 illustration: the numbered sprite SHEET up top, and a little scene built
// from those same numbers below — the hero walks (two-frame animation via frame()).
#include "studio.h"

void draw(void) {
    cls(CLR_DARK_BLUE);
    colorkey(0);                                   // sprite slot 0 (black) = see-through

    // --- the sheet: each sprite has a NUMBER ---
    print("the sheet:", 10, 8, CLR_LIGHT_GREY);
    for (int i = 0; i < 5; i++) {
        int x = 92 + i * 40;
        spr(i, x, 6);
        print(str("%d", i), x + 6, 26, CLR_YELLOW);
    }
    line(0, 44, SCREEN_W, 44, CLR_DARK_GREY);

    // --- a scene made of those numbers ---
    rectfill(0, 150, SCREEN_W, 50, CLR_DARK_GREEN); // grass
    spr(4, 34, 138); spr(4, 252, 138);             // bushes  = sprite 4
    spr(3, SCREEN_W - 26, 58);                     // a heart = sprite 3
    for (int i = 0; i < 3; i++) {                   // coins   = sprite 2, bobbing (ch.4)
        int cx = 108 + i * 42;
        int cy = 92 + (int)(sin_deg(frame() * 3 + i * 90) * 5);
        spr(2, cx, cy);
    }
    int hx = 16 + (frame() * 2) % (SCREEN_W - 48);  // the hero walks…
    int step = (frame() / 6) % 2;                   // …flipping between sprite 0 and 1
    spr(step, hx, 134);
}
