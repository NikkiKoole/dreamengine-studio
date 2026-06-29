/* de:meta
{
  "title": "17. hypotrochoid",
  "status": "active",
  "kind": [
    "tutorial"
  ],
  "teaches": [],
  "description": "A point on a small circle rolling inside a larger one traces an intricate curve. Four presets (8-loop, 5-petal, 9-loop, trefoil) — press A to cycle. Uses the parametric equation directly with cos_deg and sin_deg."
}
de:meta */
#include "studio.h"

// hypotrochoid spirograph — circle rolling inside a circle.
//
//   x(t) = (R-r)*cos(t) + d*cos((R-r)/r * t)
//   y(t) = (R-r)*sin(t) - d*sin((R-r)/r * t)
//
// R = outer circle radius
// r = inner circle radius  (must be < R)
// d = how far the pen sits from the center of the inner circle
//
// The ratio R:r (in lowest terms p:q) determines the shape:
//   the pattern closes after q full rotations and has p inner loops.
// When d > R-r the pen swings outside the inner circle — inner loops appear.
//
// press A to cycle through four presets, or tweak the numbers yourself.

//         R    r    d    cycle (degrees until pattern closes)
static float S[4][4] = {
    { 72,  27,  50,  1080 },   // 8:3 ratio — 8 loops, 3 rotations
    { 72,  45,  60,  1800 },   // 8:5 ratio — open 5-petal flower
    { 72,  32,  52,  1440 },   // 9:4 ratio — 9-loop dense rose
    { 72,  54,  72,  1080 },   // 4:3 ratio — 4-loop trefoil
};

#define DRAW_FRAMES 480.0f   // ~8 seconds to trace the full pattern (at 60fps)
#define HOLD_FRAMES 120      // hold the finished figure for ~2 seconds before redrawing

static int   pat     = 0;
static float t       = 0;
static float prev_x, prev_y;
static bool  started = false;
static int   hold    = 0;

static float spiro_x(float a) {
    float arm = S[pat][0] - S[pat][1];
    return SCREEN_W / 2.0f + arm * cos_deg(a) + S[pat][2] * cos_deg(arm / S[pat][1] * a);
}
static float spiro_y(float a) {
    float arm = S[pat][0] - S[pat][1];
    return SCREEN_H / 2.0f + arm * sin_deg(a) - S[pat][2] * sin_deg(arm / S[pat][1] * a);
}

void update() {
    if (btnp(0, BTN_A)) {
        pat = (pat + 1) % 4;
        t = 0;
        started = false;
    }
}

void draw() {
    if (!started) {
        cls(CLR_BLACK);
        prev_x = spiro_x(0);
        prev_y = spiro_y(0);
        t = 0;
        hold = 0;
        started = true;
    }

    float cycle = S[pat][3];

    if (t < cycle) {
        // advance one frame's worth of angle, drawn in small steps so the line stays smooth
        float target = t + cycle / DRAW_FRAMES;
        if (target > cycle) target = cycle;   // clamp so the last point lands exactly on t=cycle
        while (t < target) {
            t += 0.5f;
            if (t > target) t = target;       // the closing segment meets the start point cleanly
            float nx = spiro_x(t), ny = spiro_y(t);
            line((int)prev_x, (int)prev_y, (int)nx, (int)ny,
                 1 + ((int)(t / 45.0f)) % 15);
            prev_x = nx;
            prev_y = ny;
        }
    } else if (++hold > HOLD_FRAMES) {
        // finished one full round — held the closed figure, now redraw
        started = false;
    }

    print(str("pattern %d/4 — press A for next", pat + 1), 4, SCREEN_H - 12, CLR_DARK_GREY);
}
