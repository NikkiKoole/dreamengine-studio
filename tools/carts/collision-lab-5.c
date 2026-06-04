#include "studio.h"

// COLLISION LAB 5 — lines. Two tests that boxes and circles can't do:
//
//  • POINT vs SEGMENT — "how close is P to the line AB?" Project P onto the
//    segment: t = dot(AP, AB) / dot(AB, AB), CLAMPED to 0..1 (the clamp keeps
//    the answer ON the segment — past the ends, the closest point is the end
//    itself). The foot F = A + t*AB is drawn green; then it's the distance
//    test again: near the line = dist(P, F) <= thickness. The grey band IS
//    that thickness, drawn with thickline.
//
//  • SEGMENT vs SEGMENT — do AB and CD cross? No distances needed: AB crosses
//    CD exactly when C and D lie on OPPOSITE sides of line AB *and* A and B
//    lie on opposite sides of line CD. "Which side" is one cross product:
//        side(A,B,P) = (B-A) x (P-A)   — its SIGN is the side.
//    Four cross products, compare signs, done. This is the anti-tunneling
//    test: a fast bullet's PATH is a segment — test the path, not the dot,
//    and nothing teleports through walls (see the linerider cart).
//
// The mouse cursor is P. Drag any endpoint handle (A, B, C, D).
// Arrows nudge B — swing AB across CD and watch the crossing flip.

#define STATE struct GameState
#define S     ((STATE *)de_state(sizeof(STATE)))

#define THICK 12               // the "near the line" thickness
#define TOP 14
#define BOT 130

STATE {
    float ax, ay, bx, by;      // segment AB
    float cx, cy, dx_, dy_;    // segment CD
    int   drag;                // 0 none, 1..4 = A,B,C,D
    float gx, gy;
};

static void reset_game(void) {
    S->ax = 40;  S->ay = 60;  S->bx = 200; S->by = 95;
    S->cx = 250; S->cy = 35;  S->dx_ = 250; S->dy_ = 125;
    S->drag = 0;
}

void init(void) { reset_game(); }

// which side of the line AB is P on? (sign of the 2D cross product)
static float side(float ax, float ay, float bx, float by, float px, float py) {
    return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
}

void update(void) {
    int mx = mouse_x(), my = mouse_y();

    if (mouse_pressed(MOUSE_LEFT)) {
        float *hx[4] = { &S->ax, &S->bx, &S->cx, &S->dx_ };
        float *hy[4] = { &S->ay, &S->by, &S->cy, &S->dy_ };
        for (int i = 0; i < 4; i++)
            if (near(mx, my, (int)*hx[i], (int)*hy[i], 8)) {
                S->drag = i + 1; S->gx = mx - *hx[i]; S->gy = my - *hy[i];
                break;
            }
    }
    if (mouse_released(MOUSE_LEFT)) S->drag = 0;
    if (S->drag) {
        float *hx[4] = { &S->ax, &S->bx, &S->cx, &S->dx_ };
        float *hy[4] = { &S->ay, &S->by, &S->cy, &S->dy_ };
        *hx[S->drag - 1] = clamp(mx - S->gx, 6, SCREEN_W - 6);
        *hy[S->drag - 1] = clamp(my - S->gy, TOP + 4, BOT);
    }

    if (btn(0, BTN_LEFT))  S->bx -= 1.5f;
    if (btn(0, BTN_RIGHT)) S->bx += 1.5f;
    if (btn(0, BTN_UP))    S->by -= 1.5f;
    if (btn(0, BTN_DOWN))  S->by += 1.5f;
    S->bx = clamp(S->bx, 6, SCREEN_W - 6);
    S->by = clamp(S->by, TOP + 4, BOT);

#ifdef DE_TRACE
    {
        bool isect = side(S->ax, S->ay, S->bx, S->by, S->cx, S->cy) *
                     side(S->ax, S->ay, S->bx, S->by, S->dx_, S->dy_) < 0 &&
                     side(S->cx, S->cy, S->dx_, S->dy_, S->ax, S->ay) *
                     side(S->cx, S->cy, S->dx_, S->dy_, S->bx, S->by) < 0;
        watch("l", "isect=%d bx=%.0f by=%.0f", isect, S->bx, S->by);
    }
#endif
}

void draw(void) {
    cls(CLR_BLACK);
    int ax = (int)S->ax, ay = (int)S->ay, bx = (int)S->bx, by = (int)S->by;
    int cx = (int)S->cx, cy = (int)S->cy, dx2 = (int)S->dx_, dy2 = (int)S->dy_;
    int mx = mouse_x(), my = mouse_y();

    // ── point vs segment: project the mouse onto AB ──────────────────────────
    float abx = S->bx - S->ax, aby = S->by - S->ay;
    float ab2 = abx * abx + aby * aby; if (ab2 < 0.001f) ab2 = 0.001f;
    float t = ((mx - S->ax) * abx + (my - S->ay) * aby) / ab2;   // raw projection
    t = clamp(t, 0, 1);                                          // stay ON the segment
    float fx_ = S->ax + t * abx, fy_ = S->ay + t * aby;          // the foot F
    float pdx = mx - fx_, pdy = my - fy_;
    bool  on_line = pdx * pdx + pdy * pdy <= THICK * THICK;      // distance test again

    // ── segment vs segment: four cross products, compare signs ───────────────
    float s1 = side(S->ax, S->ay, S->bx, S->by, S->cx, S->cy);
    float s2 = side(S->ax, S->ay, S->bx, S->by, S->dx_, S->dy_);
    float s3 = side(S->cx, S->cy, S->dx_, S->dy_, S->ax, S->ay);
    float s4 = side(S->cx, S->cy, S->dx_, S->dy_, S->bx, S->by);
    bool  isect = s1 * s2 < 0 && s3 * s4 < 0;

    // the "near the line" band — the thickness made visible
    thickline(ax, ay, bx, by, THICK * 2, CLR_DARKER_GREY);
    // AB on top of its band
    line(ax, ay, bx, by, isect ? CLR_YELLOW : (on_line ? CLR_GREEN : CLR_WHITE));
    // CD
    line(cx, cy, dx2, dy2, isect ? CLR_YELLOW : CLR_ORANGE);

    // intersection point, when it exists (parametric form of AB)
    if (isect) {
        float denom = s1 - s2;                     // proportional trick: where CD splits AB's sides
        float u = (denom != 0) ? s1 / denom : 0.5f;
        int ix = (int)(S->cx + (S->dx_ - S->cx) * u), iy = (int)(S->cy + (S->dy_ - S->cy) * u);
        circ(ix, iy, blink(15) ? 4 : 3, CLR_YELLOW);
    }

    // the foot of the perpendicular + the mouse-P link
    line(mx, my, (int)fx_, (int)fy_, on_line ? CLR_GREEN : CLR_MEDIUM_GREY);
    circfill((int)fx_, (int)fy_, 2, CLR_GREEN);

    // endpoint handles
    int hx[4] = { ax, bx, cx, dx2 }, hy[4] = { ay, by, cy, dy2 };
    const char *hn[4] = { "A", "B", "C", "D" };
    for (int i = 0; i < 4; i++) {
        circfill(hx[i], hy[i], 3, i < 2 ? CLR_WHITE : CLR_ORANGE);
        print(hn[i], hx[i] + 5, hy[i] - 8, i < 2 ? CLR_LIGHT_GREY : CLR_ORANGE);
    }

    // readout
    font(FONT_SMALL);
    int py_ = 146;
    print(str("t = clamp(dot(AP,AB)/dot(AB,AB), 0, 1) = %.2f", t), 8, py_, CLR_WHITE);
    print(str("mouse near AB (dist<=%d)  ->  %s", THICK, on_line ? "YES" : "no"), 8, py_ + 8, on_line ? CLR_GREEN : CLR_LIGHT_GREY);
    print(str("sides of AB:  C %+.0f   D %+.0f   opposite: %s", s1, s2, s1 * s2 < 0 ? "yes" : "no "), 8, py_ + 18, CLR_WHITE);
    print(str("sides of CD:  A %+.0f   B %+.0f   opposite: %s", s3, s4, s3 * s4 < 0 ? "yes" : "no "), 8, py_ + 26, CLR_WHITE);
    print(str("both opposite -> segments cross: %s", isect ? "YES" : "no"), 8, py_ + 34, isect ? CLR_YELLOW : CLR_LIGHT_GREY);
    print("mouse = P / drag the handles / arrows swing B", 8, py_ + 44, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);

    rectfill(0, 0, SCREEN_W, 10, CLR_DARKER_BLUE);
    print("collision lab 5: lines", 4, 2, CLR_WHITE);
    print(isect ? "CROSSING" : (on_line ? "near AB" : ""), SCREEN_W - 76, 2,
          isect ? CLR_YELLOW : CLR_GREEN);
}
