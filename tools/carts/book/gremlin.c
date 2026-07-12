// Book illustration: the narrator — a friendly pixel gremlin who explains the scary bits.
#include "studio.h"

void draw(void) {
    cls(CLR_DARK_PURPLE);
    int cx = 160, cy = 118;
    // shadow
    ovalfill(cx, cy + 42, 40, 10, CLR_BLACK);
    // body
    ovalfill(cx, cy, 34, 40, CLR_GREEN);
    ovalfill(cx, cy + 4, 30, 34, CLR_GREEN);
    // belly
    ovalfill(cx, cy + 10, 20, 22, CLR_YELLOW);
    // eyes
    circfill(cx - 12, cy - 16, 9, CLR_WHITE);
    circfill(cx + 12, cy - 16, 9, CLR_WHITE);
    circfill(cx - 10, cy - 14, 4, CLR_BLACK);
    circfill(cx + 14, cy - 14, 4, CLR_BLACK);
    // little smile
    arc(cx, cy - 2, 12, 20, 160, CLR_BLACK);
    // antennae
    line(cx - 8, cy - 28, cx - 14, cy - 40, CLR_GREEN);
    line(cx + 8, cy - 28, cx + 14, cy - 40, CLR_GREEN);
    circfill(cx - 14, cy - 41, 3, CLR_RED);
    circfill(cx + 14, cy - 41, 3, CLR_RED);
    // feet
    ovalfill(cx - 14, cy + 40, 10, 5, CLR_DARK_GREEN);
    ovalfill(cx + 14, cy + 40, 10, 5, CLR_DARK_GREEN);
}
