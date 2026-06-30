/* de:meta
{
  "title": "7. keeping score",
  "status": "active",
  "created": "2026-05-29",
  "kind": [
    "tutorial"
  ],
  "teaches": [],
  "description": "Track a number and show it on screen. Press buttons to score and reset.",
  "todo": [
    "Idea: use this cart to trigger touch-overlay buttons instead of Z/X — worth discussing."
  ]
}
de:meta */
#include "studio.h"

int score = 0;
int hi    = 0;

void update() {
    if (btnp(0, BTN_A)) score++;
    if (btnp(0, BTN_B)) score = 0;
    if (score > hi) hi = score;
}

void draw() {
    cls(CLR_BROWNISH_BLACK);

    rectfill(0, 0, SCREEN_W, 24, CLR_DARK_BROWN);
    print("score:",    4,  8, CLR_LIGHT_GREY);
    print(str("%d", score), 52, 8, CLR_YELLOW);
    print("best:",  180,  8, CLR_LIGHT_GREY);
    print(str("%d", hi),  220,  8, CLR_WHITE);

    print("press Z or X to score.", 4, 40, CLR_WHITE);
    print("press the other to reset.", 4, 56, CLR_LIGHT_GREY);
    print("score is just a variable.", 4, 80, CLR_LIGHT_GREY);
    print("it remembers its value", 4, 90, CLR_LIGHT_GREY);
    print("between frames.", 4, 100, CLR_LIGHT_GREY);
}
