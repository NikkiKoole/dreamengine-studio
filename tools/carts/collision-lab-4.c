/* de:meta
{
  "title": "collision lab 4: circle vs box",
  "status": "active",
  "created": "2026-06-04",
  "kind": [
    "tutorial"
  ],
  "teaches": [
    "collision-detection"
  ],
  "genre": "lab",
  "description": "New shape pairs reduce to old tests. Circle-vs-box (every ball vs every paddle) needs no new math — just THE CLAMP TRICK: clamp the circle center to the box on each axis and you get Q, the closest point on the box (drawn yellow, sliding along the edge as you orbit). Then it is lab 2 again: dist²(center,Q) <= r², squares only. The edge case comes free — center inside the box means Q equals the center, distance zero, touching, no special case. The full algorithm is printed live with real numbers. Drag either shape, arrows nudge the circle."
}
de:meta */
#include "studio.h"

// COLLISION LAB 4 — circle vs box (ball vs paddle, player vs crate...).
// The lesson: NEW SHAPE PAIRS REDUCE TO OLD TESTS. You don't need new math
// for circle-vs-box — you need one trick and then it's lab 2 again:
//
//  1. THE CLAMP TRICK — the point on (or in) the box closest to the circle's
//     center is just the center CLAMPED to the box on each axis:
//         qx = clamp(cx, box.left, box.right)
//         qy = clamp(cy, box.top,  box.bottom)
//     That's it. Two clamps find the closest point Q (drawn yellow).
//  2. Then the question "does the circle touch the box?" becomes "is Q within
//     the circle's radius of the center?" — exactly lab 2's distance test,
//     squares only, no sqrt.
//
//  Edge case for free: if the center is INSIDE the box, the clamps change
//  nothing, Q == center, distance 0 — touching. No special case needed.
//
// Drag the circle or the box with the mouse (arrows also nudge the circle).
// Watch Q slide along the box edge as you orbit it.

#define STATE struct GameState
#define S     ((STATE *)de_state(sizeof(STATE)))

#define BW 90
#define BH 60
#define RC 26
#define TOP 16
#define BOT 134

STATE {
    float bx, by;          // box top-left
    float cx, cy;          // circle center
    int   drag;            // 0 none, 1 circle, 2 box
    float gx, gy;
};

static void reset_game(void) {
    S->bx = 60;  S->by = 50;
    S->cx = 240; S->cy = 80;
    S->drag = 0;
}

void init(void) { reset_game(); }

void update(void) {
    int mx = mouse_x(), my = mouse_y();

    if (mouse_pressed(MOUSE_LEFT)) {
        if      (near(mx, my, (int)S->cx, (int)S->cy, RC)) { S->drag = 1; S->gx = mx - S->cx; S->gy = my - S->cy; }
        else if (point_in_box(mx, my, (int)S->bx, (int)S->by, BW, BH)) { S->drag = 2; S->gx = mx - S->bx; S->gy = my - S->by; }
    }
    if (mouse_released(MOUSE_LEFT)) S->drag = 0;
    if (S->drag == 1) { S->cx = mx - S->gx; S->cy = my - S->gy; }
    if (S->drag == 2) { S->bx = mx - S->gx; S->by = my - S->gy; }

    if (btn(0, BTN_LEFT))  S->cx -= 1.5f;
    if (btn(0, BTN_RIGHT)) S->cx += 1.5f;
    if (btn(0, BTN_UP))    S->cy -= 1.5f;
    if (btn(0, BTN_DOWN))  S->cy += 1.5f;

    S->cx = clamp(S->cx, RC + 2, SCREEN_W - RC - 2);
    S->cy = clamp(S->cy, TOP + RC, BOT - RC + 10);
    S->bx = clamp(S->bx, 4, SCREEN_W - 4 - BW);
    S->by = clamp(S->by, TOP, BOT - BH);

#ifdef DE_TRACE
    {
        float qx = clamp(S->cx, S->bx, S->bx + BW), qy = clamp(S->cy, S->by, S->by + BH);
        float ddx = S->cx - qx, ddy = S->cy - qy;
        watch("l", "touch=%d d2=%.0f r2=%d", ddx * ddx + ddy * ddy <= RC * RC,
              ddx * ddx + ddy * ddy, RC * RC);
    }
#endif
}

void draw(void) {
    cls(CLR_BLACK);
    int bx = (int)S->bx, by = (int)S->by, cx = (int)S->cx, cy = (int)S->cy;

    // THE TRICK — two clamps find the closest point on the box
    int qx = (int)clamp(cx, bx, bx + BW);
    int qy = (int)clamp(cy, by, by + BH);
    int ddx = cx - qx, ddy = cy - qy;
    int d2 = ddx * ddx + ddy * ddy;
    bool touch = d2 <= RC * RC;                    // lab 2's test, reused

    // the box
    rectfill(bx, by, BW, BH, CLR_DARKER_BLUE);
    rect(bx, by, BW, BH, touch ? CLR_WHITE : CLR_BLUE);
    print("box", bx + 4, by + 3, CLR_BLUE);

    // the circle
    circfill(cx, cy, RC, CLR_DARK_BROWN);
    circ(cx, cy, RC, touch ? CLR_WHITE : CLR_ORANGE);
    pset(cx, cy, CLR_LIGHT_YELLOW);

    // Q — the closest point — and the distance line to the center
    line(cx, cy, qx, qy, touch ? CLR_YELLOW : CLR_LIGHT_GREY);
    circfill(qx, qy, 2, CLR_YELLOW);
    font(FONT_SMALL);
    print("Q", qx + 4, qy - 6, CLR_YELLOW);
    print(str("dist %.0f", fsqrt((float)d2)), (cx + qx) / 2 + 4, (cy + qy) / 2 - 8,
          touch ? CLR_YELLOW : CLR_WHITE);

    // readout — the whole algorithm, four lines
    int py_ = 150;
    print(str("qx = clamp(cx, left, right)  = clamp(%d, %d, %d) = %d", cx, bx, bx + BW, qx), 8, py_,      CLR_WHITE);
    print(str("qy = clamp(cy, top, bottom)  = clamp(%d, %d, %d) = %d", cy, by, by + BH, qy), 8, py_ + 8,  CLR_WHITE);
    print(str("dist2(center,Q) = %d   r2 = %d", d2, RC * RC), 8, py_ + 16, CLR_WHITE);
    print(str("dist2 <= r2  ->  %s", touch ? "TOUCHING" : "apart"), 8, py_ + 24, touch ? CLR_YELLOW : CLR_LIGHT_GREY);
    print("two clamps + lab 2's distance test = circle vs box", 8, py_ + 34, CLR_MEDIUM_GREY);
    print("drag either shape / arrows nudge the circle", 8, py_ + 42, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);

    rectfill(0, 0, SCREEN_W, 10, CLR_DARKER_BLUE);
    print("collision lab 4", 4, 2, CLR_WHITE);
    print(touch ? "TOUCHING" : "apart", SCREEN_W - 76, 2, touch ? CLR_YELLOW : CLR_LIGHT_GREY);
}
