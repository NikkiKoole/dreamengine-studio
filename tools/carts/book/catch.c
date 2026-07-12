// Chapter-5 payoff: a whole tiny game. Move the basket, catch the falling cherries.
// Everything from chapters 1-4 at once — cls + shapes, btn, clamping, falling motion.
// When nobody touches the controls it plays ITSELF (attract mode) — that's the gif.
#include "studio.h"

static float bx;                 // basket centre x
static float fx, fy, fvy;        // fruit position + fall speed
static int   score, flash;
static int   ready;

#define BW 52                    // basket width
#define BY (SCREEN_H - 40)       // basket top

static void spawn(void) {
    fx = rnd_between(16, SCREEN_W - 16);
    fy = -8;
    fvy = 2.0f + rnd(10) * 0.12f;
}

void init(void) { bx = SCREEN_W / 2; score = 0; flash = 0; ready = 1; spawn(); }

void update(void) {
    if (!ready) init();

    if (btn(0, BTN_LEFT))  bx -= 4;          // the player, if they want it
    else if (btn(0, BTN_RIGHT)) bx += 4;
    else bx += (fx - bx) * 0.10f;            // else the game plays itself

    float half = BW / 2;
    if (bx < half) bx = half;                // clamp — chapter 2
    if (bx > SCREEN_W - half) bx = SCREEN_W - half;

    fy += fvy;                               // fall — chapter 4
    if (fy >= BY - 2) {                      // reached the basket line
        if (fx > bx - half && fx < bx + half) { score++; flash = 8; }
        spawn();
    } else if (fy > SCREEN_H) {
        spawn();
    }
    if (flash > 0) flash--;
}

void draw(void) {
    cls(CLR_DARK_BLUE);
    line(0, BY + 18, SCREEN_W, BY + 18, CLR_DARK_GREEN);          // floor

    // the fruit — a cherry
    line((int)fx, (int)fy - 7, (int)fx + 4, (int)fy - 13, CLR_DARK_GREEN);
    circfill((int)fx, (int)fy, 7, CLR_RED);
    circfill((int)fx - 2, (int)fy - 2, 2, CLR_WHITE);            // shine

    // the basket — a little trapezoid (chapter 3's shapes)
    int half = BW / 2, l = (int)bx - half, r = (int)bx + half;
    trifill(l, BY, r, BY, r - 8, BY + 16, CLR_BROWN);
    trifill(l, BY, r - 8, BY + 16, l + 8, BY + 16, CLR_BROWN);
    line(l, BY, r, BY, CLR_ORANGE);                              // rim

    if (flash > 0) circ((int)bx, BY, half + flash, CLR_YELLOW);  // catch spark

    print(str("caught  %d", score), 8, 8, CLR_YELLOW);
}
