#include "studio.h"

void draw() {
    cls(CLR_BLACK);

    rectfill(10,  10, 50, 36, CLR_RED);
    rect    (70,  10, 50, 36, CLR_GREEN);
    circfill(155, 28, 18, CLR_BLUE);
    circ    (200, 28, 18, CLR_YELLOW);
    line    (230, 10, 310, 46, CLR_ORANGE);

    print("rectfill", 10,  48, CLR_LIGHT_GREY);
    print("rect",     70,  48, CLR_LIGHT_GREY);
    print("circfill", 136, 48, CLR_LIGHT_GREY);
    print("circ",     189, 48, CLR_LIGHT_GREY);
    print("line",     255, 48, CLR_LIGHT_GREY);

    // all 32 palette colors as a strip
    print("all 32 colors:", 10, 70, CLR_WHITE);
    for (int i = 0; i < 32; i++) {
        rectfill(10 + i * 9, 82, 8, 16, i);
    }

    // single pixels
    print("pset — single pixels:", 10, 110, CLR_WHITE);
    for (int i = 0; i < 300; i++) {
        pset(10 + (i % 300), 125 + (i / 300) * 4, i % 32);
    }
}
