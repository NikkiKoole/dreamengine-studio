// Chapter-7 mood creature: the crooner — a blob belting one out, notes rising.
#include "studio.h"

void draw(void) {
    cls(CLR_DARK_BLUE);
    int cx = SCREEN_W / 2, cy = SCREEN_H / 2 + 8;
    // rising notes
    print("\x0e", cx + 40, cy - 40, CLR_YELLOW);
    print("\x0e", cx + 56, cy - 58, CLR_ORANGE);
    print("\x0d", cx - 48, cy - 34, CLR_GREEN);
    print("\x0e", cx - 60, cy - 54, CLR_BLUE);
    ovalfill(cx, cy + 40, 40, 9, CLR_BLACK);
    ovalfill(cx, cy, 34, 40, CLR_GREEN);
    ovalfill(cx, cy + 10, 20, 22, CLR_YELLOW);
    // eyes shut, head tipped back — really feeling it
    arc(cx - 12, cy - 16, 7, 200, 340, CLR_BLACK);
    arc(cx + 12, cy - 16, 7, 200, 340, CLR_BLACK);
    ovalfill(cx, cy + 2, 6, 8, CLR_BLACK);              // wide open singing mouth
    line(cx - 8, cy - 28, cx - 14, cy - 40, CLR_GREEN);
    line(cx + 8, cy - 28, cx + 14, cy - 40, CLR_GREEN);
    circfill(cx - 14, cy - 41, 3, CLR_RED);
    circfill(cx + 14, cy - 41, 3, CLR_RED);
    ovalfill(cx - 14, cy + 39, 10, 5, CLR_DARK_GREEN);
    ovalfill(cx + 14, cy + 39, 10, 5, CLR_DARK_GREEN);
    // one arm up, holding a little microphone to the mouth
    line(cx + 30, cy, cx + 12, cy + 4, CLR_GREEN);      // arm bringing mic in
    line(cx + 30, cy - 1, cx + 44, cy - 8, CLR_DARK_GREY);
    circfill(cx + 45, cy - 9, 4, CLR_LIGHT_GREY);       // mic head
}
