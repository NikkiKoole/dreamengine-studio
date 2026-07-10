/* de:meta
{
  "slug": "shapes",
  "title": "Shapes",
  "status": "active",
  "created": "2026-06-01",
  "kind": [
    "tech-demo"
  ],
  "teaches": [],
  "description": "Showcases the geometry helpers: ngon/ngonfill (rotating polygons), star/starfill (badges and sparkles), poly/polyfill (custom outlines), thickline, rrect/rrectfill (panels and bubbles), vgradient/hgradient. Includes a fillp-dithered hexagon."
}
de:meta */
#include "studio.h"

void draw(void) {
    vgradient(0, 0, SCREEN_W, SCREEN_H, CLR_DARK_BLUE, CLR_BLACK);

    // title
    print_outline("SHAPES", (SCREEN_W - text_width("SHAPES")) / 2, 4, CLR_YELLOW, CLR_DARK_BROWN);

    float t = now();

    // ngon / ngonfill — rotating hex, triangle, gear
    ngonfill(40,  40, 18, 6,  t * 20,        CLR_DARK_GREEN);
    ngon(    40,  40, 18, 6,  t * 20,        CLR_GREEN);
    ngonfill(90,  40, 14, 3,  t * -30,       CLR_DARK_PURPLE);
    ngon(    90,  40, 14, 3,  t * -30,       CLR_PINK);
    ngonfill(140, 40, 16, 8,  t * 15,        CLR_DARK_BROWN);
    ngon(    140, 40, 16, 8,  t * 15,        CLR_ORANGE);

    // star / starfill
    starfill(195, 40, 18, 8,  5, t * 25,     CLR_DARK_ORANGE);
    star(    195, 40, 18, 8,  5, t * 25,     CLR_YELLOW);
    starfill(245, 40, 16, 6,  4, t * -20,    CLR_DARK_PURPLE);
    star(    245, 40, 16, 6,  4, t * -20,    CLR_PINK);
    starfill(295, 40, 14, 10, 6, t * 18,     CLR_DARK_BLUE);
    star(    295, 40, 14, 10, 6, t * 18,     CLR_BLUE);

    // poly / polyfill — arrow and blob
    int arrow[] = { 50,70, 80,70, 80,62, 100,80, 80,98, 80,90, 50,90 };
    polyfill(arrow, 7, CLR_DARK_BLUE);
    poly(arrow, 7, CLR_BLUE);

    int blob[] = { 140,65, 165,68, 180,80, 175,98, 155,102, 135,95, 128,78 };
    polyfill(blob, 7, CLR_DARK_GREEN);
    poly(blob, 7, CLR_GREEN);

    // thickline
    for (int i = 0; i < 5; i++)
        thickline(210, 65 + i*9, 310, 65 + i*9, i+1, CLR_BLUE + i);

    // rrect / rrectfill
    rrectfill(10, 112, 90, 30, 6, CLR_DARK_PURPLE);
    rrect(    10, 112, 90, 30, 6, CLR_PINK);
    print("speech bubble", 15, 121, CLR_BLACK);
    print("speech bubble", 14, 120, CLR_WHITE);

    rrectfill(110, 112, 60, 30, 12, CLR_DARK_BROWN);
    rrect(    110, 112, 60, 30, 12, CLR_ORANGE);

    rrectfill(180, 112, 40, 30, 4, CLR_DARKER_BLUE);
    rrect(    180, 112, 40, 30, 4, CLR_BLUE);

    // fillp + ngonfill
    fillp(FILL_CHECKER, CLR_DARK_GREEN);
    ngonfill(250, 127, 22, 6, 0, CLR_GREEN);
    fillp_reset();
    ngon(250, 127, 22, 6, 0, CLR_YELLOW);

    // hgradient
    hgradient(10, 152, SCREEN_W - 20, 14, CLR_DARK_BLUE, CLR_DARK_PURPLE);
    rrect(10, 152, SCREEN_W - 20, 14, 3, CLR_INDIGO);
    print("hgradient", 14, 155, CLR_WHITE);

    // vgradient strip on side
    vgradient(SCREEN_W - 14, 20, 10, SCREEN_H - 30, CLR_ORANGE, CLR_DARK_BLUE);
    rrect(SCREEN_W - 14, 20, 10, SCREEN_H - 30, 3, CLR_DARK_GREY);
}
