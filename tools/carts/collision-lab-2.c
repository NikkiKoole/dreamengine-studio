/* de:meta
{
  "title": "collision lab 2: circles + distance",
  "status": "active",
  "created": "2026-06-04",
  "kind": [
    "tutorial"
  ],
  "teaches": [
    "collision-detection"
  ],
  "genre": "lab",
  "description": "Two circles touch when the distance between their centers is at most the sum of their radii — and this lab draws every part of that sentence. The right triangle between the centers shows Pythagoras live (dx leg, dy leg, hypotenuse = distance); a 1:1 ruler shows the distance shrinking past the ra+rb threshold; and the readout shows the NO-SQRT trick with real numbers: dist² <= (ra+rb)² is the same question using only multiplication — which is what near() and circles_touch() actually compute. The cursor doubles as the point for point-in-circle. Drag the circles, arrows nudge A."
}
de:meta */
#include "studio.h"

// COLLISION LAB 2 — circles & distance. Two circles touch when the distance
// between their CENTERS is at most the sum of their radii. That's the whole
// test — and this lab draws every part of it:
//
//  • THE RIGHT TRIANGLE — the distance between two points is the hypotenuse
//    of a right triangle whose legs are dx and dy (Pythagoras, live on
//    screen): dist² = dx² + dy².
//
//  • THE NO-SQRT TRICK — you never need the square root. "dist <= ra+rb" is
//    exactly the same question as "dist² <= (ra+rb)²", and squares only need
//    multiplication. This is what near() and circles_touch() actually do —
//    compare the squares, never sqrt. (The readout shows both squared numbers;
//    the ruler at the bottom shows the same comparison un-squared.)
//
//  • POINT IN CIRCLE — the cursor is a point; it is "inside circle A" when
//    near(mouse, A.center, A.radius) — the same distance test with one radius.
//
// Drag the circles with the mouse (arrows also nudge circle A). Watch the
// hypotenuse shrink past the radius sum on the ruler.

#define STATE struct GameState
#define S     ((STATE *)de_state(sizeof(STATE)))

#define RA 30
#define RB 24
#define TOP 16
#define BOT 132                // play area bottom (ruler + panel below)

STATE {
    float ax, ay, bx, by;
    int   drag;
    float gx, gy;
};

static void reset_game(void) {
    S->ax = 90;  S->ay = 70;
    S->bx = 230; S->by = 90;
    S->drag = 0;
}

void init(void) { reset_game(); }

void update(void) {
    int mx = mouse_x(), my = mouse_y();

    if (mouse_pressed(MOUSE_LEFT)) {
        if      (near(mx, my, (int)S->ax, (int)S->ay, RA)) { S->drag = 1; S->gx = mx - S->ax; S->gy = my - S->ay; }
        else if (near(mx, my, (int)S->bx, (int)S->by, RB)) { S->drag = 2; S->gx = mx - S->bx; S->gy = my - S->by; }
    }
    if (mouse_released(MOUSE_LEFT)) S->drag = 0;
    if (S->drag == 1) { S->ax = mx - S->gx; S->ay = my - S->gy; }
    if (S->drag == 2) { S->bx = mx - S->gx; S->by = my - S->gy; }

    if (btn(0, BTN_LEFT))  S->ax -= 1.5f;
    if (btn(0, BTN_RIGHT)) S->ax += 1.5f;
    if (btn(0, BTN_UP))    S->ay -= 1.5f;
    if (btn(0, BTN_DOWN))  S->ay += 1.5f;

    S->ax = clamp(S->ax, RA + 2, SCREEN_W - RA - 2);
    S->ay = clamp(S->ay, TOP + RA, BOT - RA);
    S->bx = clamp(S->bx, RB + 2, SCREEN_W - RB - 2);
    S->by = clamp(S->by, TOP + RB, BOT - RB);

#ifdef DE_TRACE
    {
        float ddx = S->bx - S->ax, ddy = S->by - S->ay;
        watch("l", "touch=%d d2=%.0f r2=%d", circles_touch((int)S->ax, (int)S->ay, RA, (int)S->bx, (int)S->by, RB),
              ddx * ddx + ddy * ddy, (RA + RB) * (RA + RB));
    }
#endif
}

void draw(void) {
    cls(CLR_BLACK);
    int ax = (int)S->ax, ay = (int)S->ay, bx = (int)S->bx, by = (int)S->by;
    int mx = mouse_x(), my = mouse_y();

    int   ddx = bx - ax, ddy = by - ay;
    int   d2  = ddx * ddx + ddy * ddy;             // dist² — multiplications only
    int   r2  = (RA + RB) * (RA + RB);             // (ra+rb)²
    bool  touch = d2 <= r2;                        // == circles_touch(...)
    float dist  = fsqrt((float)d2);                // sqrt ONLY for display!

    // ── the circles ──────────────────────────────────────────────────────────
    circfill(ax, ay, RA, CLR_DARKER_BLUE);
    circ(ax, ay, RA, touch ? CLR_WHITE : CLR_BLUE);
    print("A", ax - 4, ay - RA + 5, CLR_BLUE);
    circfill(bx, by, RB, CLR_DARK_BROWN);
    circ(bx, by, RB, touch ? CLR_WHITE : CLR_ORANGE);
    print("B", bx - 4, by - RB + 5, CLR_ORANGE);

    // ── the right triangle between the centers (Pythagoras, live) ───────────
    line(ax, ay, bx, ay, CLR_MEDIUM_GREY);          // dx leg
    line(bx, ay, bx, by, CLR_MEDIUM_GREY);          // dy leg
    line(ax, ay, bx, by, touch ? CLR_YELLOW : CLR_WHITE);   // hypotenuse = distance
    pset(ax, ay, CLR_LIGHT_YELLOW); pset(bx, by, CLR_LIGHT_YELLOW);
    font(FONT_SMALL);
    print(str("dx %d", ddx), (ax + bx) / 2 - 8, ay + (ddy > 0 ? -8 : 3), CLR_LIGHT_GREY);
    print(str("dy %d", ddy), bx + (ddx > 0 ? 4 : -28), (ay + by) / 2 - 2, CLR_LIGHT_GREY);
    print(str("dist %.0f", dist), (ax + bx) / 2 + 4, (ay + by) / 2 - 8, touch ? CLR_YELLOW : CLR_WHITE);
    font(FONT_NORMAL);

    // ── the ruler: dist vs ra+rb, 1:1 in pixels ──────────────────────────────
    int ry_ = 140;
    rectfill(14, ry_, (int)dist, 3, touch ? CLR_YELLOW : CLR_LIGHT_GREY);   // the distance
    line(14 + RA + RB, ry_ - 3, 14 + RA + RB, ry_ + 5, CLR_GREEN);          // the threshold
    font(FONT_SMALL);
    print("dist", 14 + (int)dist + 4, ry_ - 1, CLR_LIGHT_GREY);
    print("ra+rb", 14 + RA + RB + 3, ry_ + 6, CLR_GREEN);

    // ── readout: the squared comparison (what the engine really does) ────────
    int py_ = 156;
    print(str("dist2     = dx*dx + dy*dy = %d", d2),  14, py_,      CLR_WHITE);
    print(str("(ra+rb)2  = %d * %d       = %d", RA + RB, RA + RB, r2), 14, py_ + 8, CLR_WHITE);
    print(str("dist2 <= (ra+rb)2  ->  %s", touch ? "TOUCHING" : "apart"), 14, py_ + 16, touch ? CLR_YELLOW : CLR_LIGHT_GREY);
    print("no sqrt needed - squares only! (this IS circles_touch)", 14, py_ + 26, CLR_MEDIUM_GREY);
    bool in_a = near(mx, my, ax, ay, RA);
    print(str("near(mouse, A, ra) = %s", in_a ? "true" : "false"), 14, py_ + 36, in_a ? CLR_GREEN : CLR_LIGHT_GREY);
    print("drag / arrows nudge A", 216, py_ + 36, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);

    // cursor crosshair — green inside circle A (the point-in-circle test)
    int cc = in_a ? CLR_GREEN : CLR_WHITE;
    line(mx - 4, my, mx + 4, my, cc);
    line(mx, my - 4, mx, my + 4, cc);

    rectfill(0, 0, SCREEN_W, 10, CLR_DARKER_BLUE);
    print("collision lab 2", 4, 2, CLR_WHITE);
    print(touch ? "TOUCHING" : "apart", SCREEN_W - 76, 2, touch ? CLR_YELLOW : CLR_LIGHT_GREY);
}
