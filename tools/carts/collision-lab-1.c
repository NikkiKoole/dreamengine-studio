/* de:meta
{
  "title": "collision lab 1: points + boxes",
  "status": "active",
  "kind": [
    "tutorial"
  ],
  "teaches": [
    "collision-detection"
  ],
  "genre": "lab",
  "description": "An interactive lab that opens up the inside of collision detection. Collision is a YES/NO question asked every frame; this cart makes the question visible. Drag two boxes with the mouse and watch their AXIS SHADOWS on the top and left margins — two boxes overlap exactly when both shadows overlap (the green segments), which is why boxes_touch() is just four comparisons with no math. The four live comparisons are printed with real numbers and flip green/red as you drag. The cursor doubles as the point for point_in_box. Mouse drags the boxes, arrows nudge box A."
}
de:meta */
#include "studio.h"

// COLLISION LAB 1 — points & boxes. Collision is just a YES/NO question asked
// every frame: "do these two things overlap right now?" This lab makes the
// inside of that question visible.
//
//  • POINT IN BOX — the cursor is the point. It is inside box A when FOUR
//    comparisons all pass: right of the left edge, left of the right edge,
//    below the top, above the bottom. (point_in_box)
//
//  • BOX VS BOX (AABB) — the genius reduction: two boxes overlap exactly when
//    their X-INTERVALS overlap AND their Y-INTERVALS overlap. Look at the bars
//    along the top and left margins: each box casts a shadow onto each axis,
//    and the boxes touch only while BOTH shadows overlap (the green segments).
//    Each interval-overlap is two comparisons, so the whole test is four:
//        A.left < B.right    A.right > B.left
//        A.top  < B.bottom   A.bottom > B.top
//    All four pass = overlap (the yellow checkered region). One fails = no.
//    That is the entire boxes_touch() function — four comparisons, no math.
//
// Drag the boxes with the mouse (arrows also nudge box A). Watch the four
// conditions flip as the shadows slide past each other.

#define STATE struct GameState
#define S     ((STATE *)de_state(sizeof(STATE)))

// ── box sizes (positions live in STATE so they hot-reload) ───────────────────
#define AW 64
#define AH 44
#define BW 52
#define BH 52
#define TOP 30                 // play area: below the margin bars
#define BOT 146                // play area bottom: above the readout panel

STATE {
    float ax, ay, bx, by;
    int   drag;                // 0 none, 1 = box A, 2 = box B
    float gx, gy;              // grab offset
};

static void reset_game(void) {
    S->ax = 70;  S->ay = 60;
    S->bx = 190; S->by = 80;
    S->drag = 0;
}

void init(void) { reset_game(); }

void update(void) {
    int mx = mouse_x(), my = mouse_y();

    // pick up / drag / drop with the mouse
    if (mouse_pressed(MOUSE_LEFT)) {
        if      (point_in_box(mx, my, (int)S->ax, (int)S->ay, AW, AH)) { S->drag = 1; S->gx = mx - S->ax; S->gy = my - S->ay; }
        else if (point_in_box(mx, my, (int)S->bx, (int)S->by, BW, BH)) { S->drag = 2; S->gx = mx - S->bx; S->gy = my - S->by; }
    }
    if (mouse_released(MOUSE_LEFT)) S->drag = 0;
    if (S->drag == 1) { S->ax = mx - S->gx; S->ay = my - S->gy; }
    if (S->drag == 2) { S->bx = mx - S->gx; S->by = my - S->gy; }

    // arrows nudge box A (keyboard / harness driving)
    if (btn(0, BTN_LEFT))  S->ax -= 1.5f;
    if (btn(0, BTN_RIGHT)) S->ax += 1.5f;
    if (btn(0, BTN_UP))    S->ay -= 1.5f;
    if (btn(0, BTN_DOWN))  S->ay += 1.5f;

    S->ax = clamp(S->ax, 10, SCREEN_W - 10 - AW);
    S->ay = clamp(S->ay, TOP, BOT - AH);
    S->bx = clamp(S->bx, 10, SCREEN_W - 10 - BW);
    S->by = clamp(S->by, TOP, BOT - BH);

#ifdef DE_TRACE
    watch("l", "touch=%d ax=%.0f ay=%.0f bx=%.0f by=%.0f",
          boxes_touch((int)S->ax, (int)S->ay, AW, AH, (int)S->bx, (int)S->by, BW, BH),
          S->ax, S->ay, S->bx, S->by);
#endif
}

void draw(void) {
    cls(CLR_BLACK);
    int ax = (int)S->ax, ay = (int)S->ay, bx = (int)S->bx, by = (int)S->by;
    int mx = mouse_x(), my = mouse_y();

    // the four comparisons, spelled out
    bool c1 = ax      < bx + BW;     // A.left   < B.right
    bool c2 = ax + AW > bx;          // A.right  > B.left
    bool c3 = ay      < by + BH;     // A.top    < B.bottom
    bool c4 = ay + AH > by;          // A.bottom > B.top
    bool touch = c1 && c2 && c3 && c4;             // == boxes_touch(...)

    // ── axis shadows: each box projected onto the top and left margins ──────
    // X intervals (top margin)
    rectfill(ax, 14, AW, 3, CLR_TRUE_BLUE);
    rectfill(bx, 19, BW, 3, CLR_DARK_ORANGE);
    if (c1 && c2) {                                // X shadows overlap
        int x0 = ax > bx ? ax : bx, x1 = (ax + AW < bx + BW) ? ax + AW : bx + BW;
        rectfill(x0, 24, x1 - x0, 3, CLR_GREEN);
    }
    // Y intervals (left margin)
    rectfill(2, ay, 3, AH, CLR_TRUE_BLUE);
    rectfill(7, by, 3, BH, CLR_DARK_ORANGE);
    if (c3 && c4) {                                // Y shadows overlap
        int y0 = ay > by ? ay : by, y1 = (ay + AH < by + BH) ? ay + AH : by + BH;
        rectfill(12, y0, 3, y1 - y0, CLR_GREEN);
    }

    // ── the boxes ────────────────────────────────────────────────────────────
    rectfill(ax, ay, AW, AH, CLR_DARKER_BLUE);
    rect(ax, ay, AW, AH, touch ? CLR_WHITE : CLR_BLUE);
    print("A", ax + 4, ay + 3, CLR_BLUE);
    rectfill(bx, by, BW, BH, CLR_DARK_BROWN);
    rect(bx, by, BW, BH, touch ? CLR_WHITE : CLR_ORANGE);
    print("B", bx + 4, by + 3, CLR_ORANGE);

    // the overlap region — only exists when ALL FOUR comparisons pass
    if (touch) {
        int x0 = ax > bx ? ax : bx, x1 = (ax + AW < bx + BW) ? ax + AW : bx + BW;
        int y0 = ay > by ? ay : by, y1 = (ay + AH < by + BH) ? ay + AH : by + BH;
        fillp(FILL_CHECKER, -1);
        rectfill(x0, y0, x1 - x0, y1 - y0, CLR_YELLOW);
        fillp_reset();
    }

    // ── point-in-box: the cursor is the point ────────────────────────────────
    bool in_a = point_in_box(mx, my, ax, ay, AW, AH);
    bool in_b = point_in_box(mx, my, bx, by, BW, BH);
    int cc = (in_a || in_b) ? CLR_GREEN : CLR_WHITE;
    line(mx - 4, my, mx + 4, my, cc);
    line(mx, my - 4, mx, my + 4, cc);

    // ── readout panel: the four live comparisons ─────────────────────────────
    font(FONT_SMALL);
    int py_ = 152;
    print(str("A.left  %3d < B.right  %3d", ax, bx + BW),      14, py_,      c1 ? CLR_GREEN : CLR_RED);
    print(str("A.right %3d > B.left   %3d", ax + AW, bx),      14, py_ + 8,  c2 ? CLR_GREEN : CLR_RED);
    print(str("A.top   %3d < B.bottom %3d", ay, by + BH),      14, py_ + 16, c3 ? CLR_GREEN : CLR_RED);
    print(str("A.bottom%3d > B.top    %3d", ay + AH, by),      14, py_ + 24, c4 ? CLR_GREEN : CLR_RED);
    print("all four pass = overlap", 14, py_ + 34, CLR_LIGHT_GREY);
    print(str("point_in_box(mouse, A) = %s", in_a ? "true" : "false"), 170, py_, in_a ? CLR_GREEN : CLR_LIGHT_GREY);
    print(str("point_in_box(mouse, B) = %s", in_b ? "true" : "false"), 170, py_ + 8, in_b ? CLR_GREEN : CLR_LIGHT_GREY);
    print("drag boxes with the mouse", 170, py_ + 20, CLR_MEDIUM_GREY);
    print("arrows nudge box A", 170, py_ + 28, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);

    rectfill(0, 0, SCREEN_W, 10, CLR_DARKER_BLUE);
    print("collision lab 1", 4, 2, CLR_WHITE);
    print(touch ? "TOUCHING" : "apart", SCREEN_W - 76, 2, touch ? CLR_YELLOW : CLR_LIGHT_GREY);
}
