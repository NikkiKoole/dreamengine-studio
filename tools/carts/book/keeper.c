// Chapter-12 mood creature: the keeper — a blob guarding a chest with its best score.
#include "studio.h"

void draw(void) {
    cls(CLR_DARK_BLUE);
    int cx = SCREEN_W / 2 - 8, cy = SCREEN_H / 2 + 8;
    // the treasure chest, lid open, a golden number floating out
    int chx = cx + 40, chy = cy + 4;
    rectfill(chx, chy, 52, 30, CLR_BROWN);              // box
    rect(chx, chy, 52, 30, CLR_DARK_GREY);
    trifill(chx, chy, chx + 52, chy, chx + 46, chy - 14, CLR_DARK_GREY);  // open lid
    trifill(chx, chy, chx + 46, chy - 14, chx + 6, chy - 14, CLR_DARK_GREY);
    rectfill(chx + 22, chy + 10, 8, 8, CLR_YELLOW);     // lock
    print_scaled("42", chx + 12, chy - 40, CLR_YELLOW, 2);   // the saved number, rising
    line(chx + 20, chy - 6, chx + 22, chy - 20, CLR_YELLOW); // sparkle rays
    // the blob, proud beside it
    ovalfill(cx, cy + 40, 40, 9, CLR_BLACK);
    ovalfill(cx, cy, 34, 40, CLR_GREEN);
    ovalfill(cx, cy + 10, 20, 22, CLR_YELLOW);
    circfill(cx - 12, cy - 16, 9, CLR_WHITE);
    circfill(cx + 12, cy - 16, 9, CLR_WHITE);
    circfill(cx - 10, cy - 16, 4, CLR_BLACK);
    circfill(cx + 14, cy - 16, 4, CLR_BLACK);
    arc(cx, cy - 2, 12, 20, 160, CLR_BLACK);
    line(cx - 8, cy - 28, cx - 14, cy - 40, CLR_GREEN);
    line(cx + 8, cy - 28, cx + 14, cy - 40, CLR_GREEN);
    circfill(cx - 14, cy - 41, 3, CLR_RED);
    circfill(cx + 14, cy - 41, 3, CLR_RED);
    ovalfill(cx - 14, cy + 39, 10, 5, CLR_DARK_GREEN);
    ovalfill(cx + 14, cy + 39, 10, 5, CLR_DARK_GREEN);
    // a hand resting proudly on the chest
    line(cx + 28, cy + 4, chx, chy - 2, CLR_GREEN);
}
