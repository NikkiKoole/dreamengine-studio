// Book illustration: "the whole crayon box" — all 32 palette colors, numbered.
#include "studio.h"

void draw(void) {
    cls(CLR_DARK_BLUE);
    int cols = 8, sw = 34, sh = 40, ox = 24, oy = 12;
    for (int i = 0; i < 32; i++) {
        int cx = ox + (i % cols) * sw;
        int cy = oy + (i / cols) * sh;
        rectfill(cx, cy, sw - 6, sh - 8, i);
        // number label, readable on light or dark swatch
        int ink = (i == 0 || i == 1 || i == 2 || i == 5 || i == 13 || i == 3) ? CLR_WHITE : CLR_BLACK;
        char n[4]; n[0] = '0' + (i / 10); n[1] = '0' + (i % 10); n[2] = 0;
        print(i < 10 ? n + 1 : n, cx + 3, cy + 3, ink);
    }
}
