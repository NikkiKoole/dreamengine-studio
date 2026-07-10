/* de:meta
{
  "slug": "02-shapes",
  "title": "2. shapes and colors",
  "status": "active",
  "created": "2026-05-29",
  "kind": [
    "tutorial"
  ],
  "teaches": [],
  "description": "Draw rectangles, circles, lines, and pixels in all 32 palette colors."
}
de:meta */
#include "studio.h"

void draw() {
    cls(CLR_BLACK);

    rectfill(10,  10, 50, 36, CLR_RED);
    rect    (70,  10, 50, 36, CLR_GREEN);
    circfill(155, 28, 18, CLR_BLUE);
    circ    (200, 28, 18, CLR_YELLOW);
    line    (230, 10, 290, 46, CLR_ORANGE);
    trifill (10,  60, 50, 100, 1,  100, CLR_PINK);
    tri     (60,  60, 100, 100, 51, 100, CLR_BLUE);

    print("rectfill", 10,  103, CLR_LIGHT_GREY);
    print("rect",     68,  103, CLR_LIGHT_GREY);
    print("circfill", 136, 103, CLR_LIGHT_GREY);
    print("circ",     189, 103, CLR_LIGHT_GREY);
    print("line",     233, 103, CLR_LIGHT_GREY);
    print("trifill",  8,   48,  CLR_LIGHT_GREY);
    print("tri",      61,  48,  CLR_LIGHT_GREY);

    // all 32 palette colors as a strip
    print("all 32 colors:", 10, 118, CLR_WHITE);
    for (int i = 0; i < 32; i++) {
        rectfill(10 + i * 9, 130, 8, 16, i);
    }

    // single pixels
    print("pset:", 10, 158, CLR_WHITE);
    for (int i = 0; i < 300; i++) {
        pset(10 + (i % 300), 168 + (i / 300) * 4, i % 32);
    }
}
