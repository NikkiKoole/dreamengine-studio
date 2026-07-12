// Book illustration (ch.2): a ball that ACTUALLY bounces, leaving a comet trail —
// we never clear the screen, we just fade it a little each frame (the Chapter 1
// secret, spent). One clean traversal every ~96 frames.
#include "studio.h"

static float bx, by, vx, vy;
static int   ready;

void init(void) { bx = 6; by = 30; vx = 3.4f; vy = 0; ready = 1; }

void update(void) {
    if (!ready) init();
    float ground = 142, r = 8;
    vy += 0.55f;                 // gravity
    by += vy;
    bx += vx;
    if (by > ground - r) {                       // hit the floor
        by = ground - r;
        vy = -vy * 0.82f;                        // bounce, losing a little each time
        if (vy > -8.0f) vy = -9.5f;              // never let it die — keep it lively
    }
    if (bx > 316) init();                        // off the right edge → fresh drop on the left
}

void draw(void) {
    if (frame() == 0) cls(CLR_BLACK);
    fade(0.14f);                                 // decay the whole screen → the trail
    line(0, 152, 320, 152, CLR_DARK_GREEN);      // keep the floor crisp
    circfill((int)bx, (int)by, 8, CLR_YELLOW);
    circ((int)bx, (int)by, 8, CLR_ORANGE);
}
