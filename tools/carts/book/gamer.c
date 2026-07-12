// Chapter-14 mood creature: the gamer — a blob happily playing its game on a phone.
#include "studio.h"

void draw(void) {
    cls(CLR_DARK_BLUE);
    int cx = SCREEN_W / 2, cy = SCREEN_H / 2 - 2;
    // the blob
    ovalfill(cx, cy + 42, 42, 9, CLR_BLACK);
    ovalfill(cx, cy, 36, 42, CLR_GREEN);
    ovalfill(cx, cy + 12, 20, 22, CLR_YELLOW);
    circfill(cx - 13, cy - 18, 9, CLR_WHITE);
    circfill(cx + 13, cy - 18, 9, CLR_WHITE);
    circfill(cx - 11, cy - 18, 4, CLR_BLACK);           // eyes down on the screen
    circfill(cx + 15, cy - 18, 4, CLR_BLACK);
    arc(cx, cy - 4, 12, 20, 160, CLR_BLACK);
    line(cx - 8, cy - 30, cx - 14, cy - 42, CLR_GREEN);
    line(cx + 8, cy - 30, cx + 14, cy - 42, CLR_GREEN);
    circfill(cx - 14, cy - 43, 3, CLR_RED);
    circfill(cx + 14, cy - 43, 3, CLR_RED);
    ovalfill(cx - 14, cy + 41, 10, 5, CLR_DARK_GREEN);
    ovalfill(cx + 14, cy + 41, 10, 5, CLR_DARK_GREEN);
    // the phone, held out in front (landscape), with a tiny game on it
    int px = cx - 34, py = cy + 6, pw = 68, ph = 36;
    rectfill(px - 3, py - 3, pw + 6, ph + 6, CLR_DARK_GREY);   // body
    rectfill(px, py, pw, ph, CLR_DARK_BLUE);                   // screen
    rectfill(px, py + ph - 8, pw, 8, CLR_DARK_GREEN);          // little ground
    circfill(px + 20, py + ph - 14, 4, CLR_RED);               // a tiny sprite
    circfill(px + 46, py + 10, 3, CLR_YELLOW);                 // a tiny coin
    // hands gripping the ends
    circfill(px - 4, py + ph / 2, 6, CLR_GREEN);
    circfill(px + pw + 4, py + ph / 2, 6, CLR_GREEN);
}
