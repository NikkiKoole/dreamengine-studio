// Chapter-9 illustration: sixty-four dots, each with its own position and velocity,
// all bouncing around a box — drawn by ONE loop over arrays.
#include "studio.h"

#define N 64
static float x[N], y[N], vx[N], vy[N];
static int   col[N], ready;

void init(void) {
    for (int i = 0; i < N; i++) {
        x[i] = rnd(SCREEN_W); y[i] = rnd(SCREEN_H);
        vx[i] = (rnd(200) - 100) / 45.0f;
        vy[i] = (rnd(200) - 100) / 45.0f;
        if (vx[i] > -0.4f && vx[i] < 0.4f) vx[i] = 1.0f;   // no lazy dots
        col[i] = 8 + rnd(8);                               // a bright colour each
    }
    ready = 1;
}

void update(void) {
    if (!ready) init();
    for (int i = 0; i < N; i++) {                          // the whole cast, one loop
        x[i] += vx[i]; y[i] += vy[i];
        if (x[i] < 5 || x[i] > SCREEN_W - 5) vx[i] = -vx[i];
        if (y[i] < 5 || y[i] > SCREEN_H - 5) vy[i] = -vy[i];
    }
}

void draw(void) {
    cls(CLR_DARK_BLUE);
    for (int i = 0; i < N; i++) circfill((int)x[i], (int)y[i], 4, col[i]);
    print(str("%d dots, one for-loop", N), 8, 8, CLR_LIGHT_GREY);
}
