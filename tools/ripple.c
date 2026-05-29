#include "studio.h"

// RIPPLE TANK — water, and how waves add up.
//
// Each cell holds a water height. The wave rule: a cell's next height is the
// average of its four neighbours, minus its own previous height (momentum),
// damped a touch so ripples fade. Drop a stone and a ring spreads out; drop
// two and the rings cross — where crests meet they pile up, where crest meets
// trough they cancel. That's interference, the same maths as sound and light.
//
//   move the stone: arrows     Z: drop it     (rain falls on its own)

#define GW   160
#define GH   100
#define CELL 2

static float a[GW * GH], b[GW * GH];
static int   curx = GW / 2, cury = GH / 2, t;

static void drop(int x, int y, float amp) {
    if (x < 1 || x >= GW - 1 || y < 1 || y >= GH - 1) return;
    a[y * GW + x] += amp;
}

static void step(void) {
    for (int y = 1; y < GH - 1; y++)
        for (int x = 1; x < GW - 1; x++) {
            int i = y * GW + x;
            float v = (a[i - 1] + a[i + 1] + a[i - GW] + a[i + GW]) * 0.5f - b[i];
            b[i] = v * 0.985f;
        }
    for (int i = 0; i < GW * GH; i++) { float tmp = a[i]; a[i] = b[i]; b[i] = tmp; }
}

void init(void) {
    for (int i = 0; i < GW * GH; i++) a[i] = b[i] = 0;
    drop(50, 40, 220); drop(110, 62, 220); drop(82, 28, 170);
    for (int i = 0; i < 22; i++) step();
}

void update(void) {
    if (btn(0, BTN_LEFT))  curx = max(1, curx - 2);
    if (btn(0, BTN_RIGHT)) curx = min(GW - 2, curx + 2);
    if (btn(0, BTN_UP))    cury = max(1, cury - 2);
    if (btn(0, BTN_DOWN))  cury = min(GH - 2, cury + 2);
    if (btnp(0, BTN_A)) drop(curx, cury, 320);
    if (++t % 55 == 0) drop(rnd(GW - 2) + 1, rnd(GH - 2) + 1, 180);   // rain
    step();
}

void draw(void) {
    cls(CLR_DARKER_BLUE);
    for (int y = 0; y < GH; y++)
        for (int x = 0; x < GW; x++) {
            float v = a[y * GW + x];
            int col = v > 40 ? CLR_WHITE : v > 12 ? CLR_BLUE : v > -4 ? CLR_TRUE_BLUE
                    : v > -28 ? CLR_DARK_BLUE : CLR_DARKER_BLUE;
            rectfill(x * CELL, y * CELL, CELL, CELL, col);
        }
    rect(curx * CELL - 2, cury * CELL - 2, 5, 5, CLR_YELLOW);
    print("RIPPLE TANK", 4, 4, CLR_LIGHT_GREY);
    print("arrows: move stone   Z: drop", 4, SCREEN_H - 9, CLR_DARK_GREY);
}
