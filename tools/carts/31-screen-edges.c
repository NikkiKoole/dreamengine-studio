/* de:meta
{
  "slug": "31-screen-edges",
  "title": "31. hitting the edges",
  "status": "active",
  "created": "2026-07-11",
  "kind": [
    "tutorial"
  ],
  "teaches": [],
  "description": "Three ways to handle a mover that reaches the screen edge: clamp, bounce, or wrap."
}
de:meta */
#include "studio.h"

// A ball drifts across the screen on its own. Sooner or later it hits an edge.
// What SHOULD happen? There are three classic answers, and picking one is a
// real game-feel decision:
//   CLAMP  — stop dead at the edge (a wall). Use clamp(v, lo, hi).
//   BOUNCE — flip the velocity so it heads back (a rubber wall).
//   WRAP   — vanish off one side and reappear on the opposite side (Asteroids).
// Press Z (BTN_A) to cycle the mode and watch the SAME ball behave differently.

#define R 6                 // ball radius
#define CLAMP  0
#define BOUNCE 1
#define WRAP   2

float bx, by;               // ball position
float vx, vy;               // ball velocity (pixels per frame)
int   mode;                 // which edge rule is active right now

void init(void) {
    bx = SCREEN_W / 2.0f;
    by = SCREEN_H / 2.0f;
    vx = 1.8f;              // a gentle diagonal drift so it reaches every edge
    vy = 1.3f;
    mode = CLAMP;
}

void update() {
    // Z (or gamepad A) cycles the mode. btnp() is true ONLY on the press frame,
    // so one tap = one step (not 60 steps a second while the key is held).
    if (btnp(0, BTN_A)) {
        mode = (mode + 1) % 3;
        note(72, INSTR_TRI, 4);        // a click so the mode change is FELT
    }

    // move first...
    bx += vx;
    by += vy;

    // ...then apply the chosen edge rule.
    if (mode == CLAMP) {
        // Pin the position inside the screen. clamp() keeps a value in a range.
        // The ball simply parks against the wall — its velocity never changes.
        bx = clamp(bx, R, SCREEN_W - R);
        by = clamp(by, R, SCREEN_H - R);
    } else if (mode == BOUNCE) {
        // Passed an edge? Flip that velocity so the ball turns around.
        if (bx < R || bx > SCREEN_W - R) { vx = -vx; note(60, INSTR_TRI, 3); }
        if (by < R || by > SCREEN_H - R) { vy = -vy; note(60, INSTR_TRI, 3); }
        bx = clamp(bx, R, SCREEN_W - R);   // and nudge back in so it can't stick
        by = clamp(by, R, SCREEN_H - R);
    } else { // WRAP
        // Off the left edge -> come in on the right, and so on. The world loops.
        if (bx < -R)          bx = SCREEN_W + R;
        if (bx > SCREEN_W + R) bx = -R;
        if (by < -R)          by = SCREEN_H + R;
        if (by > SCREEN_H + R) by = -R;
    }
}

void draw() {
    cls(CLR_DARK_BLUE);

    // The ball glows the mode colour so the rule is felt at a glance.
    int col = (mode == CLAMP) ? CLR_ORANGE : (mode == BOUNCE) ? CLR_GREEN : CLR_INDIGO;
    circfill((int)bx, (int)by, R, col);

    // HUD: name the concept + its live state, and describe what it does.
    rectfill(0, 0, SCREEN_W, 26, CLR_DARKER_BLUE);
    const char *name = (mode == CLAMP) ? "CLAMP" : (mode == BOUNCE) ? "BOUNCE" : "WRAP";
    const char *desc = (mode == CLAMP)  ? "stop dead at the edge"
                     : (mode == BOUNCE) ? "flip velocity, bounce back"
                                        : "reappear on the far side";
    print(str("mode: %s", name), 4, 4, col);
    print(desc, 4, 14, CLR_LIGHT_GREY);
    print("Z: next mode", 216, 4, CLR_WHITE);
}
