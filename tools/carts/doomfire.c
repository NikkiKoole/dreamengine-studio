/* de:meta
{
  "title": "doom fire",
  "status": "active",
  "created": "2026-05-30",
  "kind": [
    "tech-demo"
  ],
  "teaches": [
    "cellular-automata"
  ],
  "description": "The famous 1993 fire effect from one tiny rule. A grid of heat values: the bottom row is always hottest, and each cell copies the heat below it minus a little random cooling, nudged by the wind. No flame is ever drawn — it emerges from the cooling. Hotter values map to hotter colours. Left/right steer the wind, up/down change flame height."
}
de:meta */
#include "studio.h"

// DOOM FIRE — the famous 1993 flame effect, built from one tiny rule.
//
// Keep a grid of "heat" numbers. The bottom row is always max heat (the coals).
// Every frame, each cell copies the heat from the cell below it, minus a random
// bit of cooling, nudged sideways by the wind. Hotter numbers map to hotter
// colours (black -> red -> orange -> yellow -> white). That's the whole thing:
// no flame is ever drawn, it just emerges from the cooling.
//
//   left / right   wind direction      up / down   flame height

#define GW   80
#define GH   50
#define CELL 4
#define NRAMP 10

static int heat[GW * GH];
static int wind = 0, cool = 1;
static const int RAMP[NRAMP] = {
    CLR_BLACK, CLR_BROWNISH_BLACK, CLR_DARK_RED, CLR_RED, CLR_DARK_ORANGE,
    CLR_ORANGE, CLR_DARK_PEACH, CLR_YELLOW, CLR_LIGHT_YELLOW, CLR_WHITE,
};

static void step(void) {
    for (int x = 0; x < GW; x++) heat[(GH - 1) * GW + x] = NRAMP - 1;   // the coals
    for (int y = 1; y < GH; y++)
        for (int x = 0; x < GW; x++) {
            int below = heat[y * GW + x];
            int nx = mid(0, x + (rnd(3) - 1) + wind, GW - 1);
            int v = below - rnd(cool + 1); if (v < 0) v = 0;
            heat[(y - 1) * GW + nx] = v;
        }
}

void init(void) {
    for (int i = 0; i < GW * GH; i++) heat[i] = 0;
    for (int i = 0; i < 120; i++) step();   // pre-warm so it's a full blaze on screen
}

void update(void) {
    wind = btn(0, BTN_LEFT) ? -1 : btn(0, BTN_RIGHT) ? 1 : 0;
    if (btnp(0, BTN_UP))   cool = max(1, cool - 1);
    if (btnp(0, BTN_DOWN)) cool = min(4, cool + 1);
    step();
}

void draw(void) {
    cls(CLR_BLACK);
    for (int y = 0; y < GH; y++)
        for (int x = 0; x < GW; x++) {
            int h = heat[y * GW + x];
            if (h > 0) rectfill(x * CELL, y * CELL, CELL, CELL, RAMP[h]);
        }
    print("DOOM FIRE", 4, 4, CLR_LIGHT_GREY);
    print("left/right: wind   up/down: flame height", 4, SCREEN_H - 9, CLR_DARK_GREY);
}
