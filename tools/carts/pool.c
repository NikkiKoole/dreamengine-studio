// POOL — 8-ball-ish billiards on one table, 2-player hotseat.
//
// Aim with the MOUSE (a dotted guide shows the cue ball's path), HOLD the left
// button to charge power (a meter fills), RELEASE to strike. Honest 2D physics:
// equal-mass elastic ball-to-ball collisions, rail bounces with restitution, and
// rolling friction so the table settles on its own. Balls that roll over a pocket
// mouth sink; sinking the cue ball is a SCRATCH. Pot an object ball and you shoot
// again; a dry shot or a scratch passes the turn. Clear the table to win.
//
//   MOUSE aim   HOLD-left charge / RELEASE shoot   RIGHT-CLICK or SPACE cancel
//   R restart
//
// Drawn entirely from primitives — no sprite sheet. 320x200.

#include <stddef.h>
#include "studio.h"

// ---------------------------------------------------------------- tuning
#define BALL_R       4
#define NBALL        16            // [0] = cue, [1..15] = object balls
#define FRICTION     0.62f         // velocity multiplier per second (rolling drag)
#define REST_RAIL    0.72f         // rail bounciness
#define STOP_SPD     6.0f          // below this speed a ball is parked
#define MAX_POWER    420.0f        // launch speed at full charge
#define SUBSTEPS     6
#define POCKET_R     7

// table felt rectangle (inside the rails)
#define T_X1  28
#define T_Y1  30
#define T_X2  292
#define T_Y2  178
#define RAIL  10                   // wooden rail thickness around the felt

// ---------------------------------------------------------------- types
typedef struct { float x, y, vx, vy; bool alive; int col; } Ball;

// ---------------------------------------------------------------- state
static Ball balls[NBALL];
static int  gstate;                // 0 aim, 1 charging, 2 rolling, 3 win
static int  cur;                   // whose turn (0 / 1)
static int  potted[2];             // running pot tally per player
static bool pottedThisShot;        // any object ball sunk this shot?
static bool scratched;             // cue ball sunk this shot?
static bool firstShot;             // the break
static float charge;               // 0..1 power
static float chargeDir;            // power meter oscillation direction
static int  hiPots;                // best single-game total (save)
static float winFlash;

// the 6 pocket mouths (4 corners + 2 side)
static const int PKx[6] = { T_X1, (T_X1+T_X2)/2, T_X2, T_X1, (T_X1+T_X2)/2, T_X2 };
static const int PKy[6] = { T_Y1, T_Y1,          T_Y1, T_Y2, T_Y2,          T_Y2 };

// object-ball colors, vaguely pool-like (cue is white, handled separately)
static const int BCOL[15] = {
    CLR_YELLOW, CLR_BLUE, CLR_RED, CLR_DARK_PURPLE, CLR_ORANGE,
    CLR_DARK_GREEN, CLR_DARK_RED, CLR_BLACK,                 // 8-ball
    CLR_LIGHT_YELLOW, CLR_TRUE_BLUE, CLR_DARK_ORANGE, CLR_INDIGO,
    CLR_PINK, CLR_LIME_GREEN, CLR_BROWN
};

// ---------------------------------------------------------------- helpers
static float speed_of(Ball *b) { return fsqrt(b->vx * b->vx + b->vy * b->vy); }

static bool any_moving(void) {
    for (int i = 0; i < NBALL; i++)
        if (balls[i].alive && speed_of(&balls[i]) > STOP_SPD) return true;
    return false;
}

static int object_balls_left(void) {
    int n = 0;
    for (int i = 1; i < NBALL; i++) if (balls[i].alive) n++;
    return n;
}

// rack the 15 object balls in a triangle pointing left, cue ball on the head spot
static void rack(void) {
    for (int i = 0; i < NBALL; i++) { balls[i].alive = false; balls[i].vx = balls[i].vy = 0; }

    // cue ball
    balls[0].x = T_X1 + (T_X2 - T_X1) * 0.25f;
    balls[0].y = (T_Y1 + T_Y2) / 2.0f;
    balls[0].col = CLR_WHITE;
    balls[0].alive = true;

    // triangle apex on the foot spot, opening toward the cue
    float apexx = T_X1 + (T_X2 - T_X1) * 0.70f;
    float apexy = (T_Y1 + T_Y2) / 2.0f;
    float gap = BALL_R * 2.0f + 0.5f;
    int idx = 1;
    for (int row = 0; row < 5; row++) {
        for (int k = 0; k <= row; k++) {
            if (idx >= NBALL) break;
            balls[idx].x = apexx + row * gap * 0.866f;
            balls[idx].y = apexy + (k - row / 2.0f) * gap;
            balls[idx].col = BCOL[idx - 1];
            balls[idx].alive = true;
            idx++;
        }
    }
}

static void new_game(void) {
    rack();
    cur = 0; potted[0] = potted[1] = 0;
    charge = 0; chargeDir = 1;
    firstShot = true;
    gstate = 0;
    winFlash = 0;
}

void init(void) {
    // SFX instruments (slots 5..15)
    instrument(5, INSTR_SINE,   1, 40, 0, 60);    // ball-ball click
    instrument(6, INSTR_TRI,    1, 70, 0, 90);    // rail thunk
    instrument(7, INSTR_SINE,   2, 90, 2, 160);   // pocket drop
    instrument(8, INSTR_SQUARE, 3, 60, 3, 220);   // scratch sting
    instrument_lfo(8, 0, LFO_PITCH, 7.0f, 1.5f);
    instrument(9, INSTR_NOISE,  1, 50, 0, 70);    // break crack

    hiPots = load_int("poolhi", 0);
    new_game();
}

// resolve one ball against the four rails (felt bounds), with restitution
static void rail_bounce(Ball *b) {
    if (b->x - BALL_R < T_X1) { b->x = T_X1 + BALL_R; if (b->vx < 0) { b->vx = -b->vx * REST_RAIL; if (speed_of(b) > 30) { hit(48, 6, 3, 70); } } }
    if (b->x + BALL_R > T_X2) { b->x = T_X2 - BALL_R; if (b->vx > 0) { b->vx = -b->vx * REST_RAIL; if (speed_of(b) > 30) { hit(48, 6, 3, 70); } } }
    if (b->y - BALL_R < T_Y1) { b->y = T_Y1 + BALL_R; if (b->vy < 0) { b->vy = -b->vy * REST_RAIL; if (speed_of(b) > 30) { hit(48, 6, 3, 70); } } }
    if (b->y + BALL_R > T_Y2) { b->y = T_Y2 - BALL_R; if (b->vy > 0) { b->vy = -b->vy * REST_RAIL; if (speed_of(b) > 30) { hit(48, 6, 3, 70); } } }
}

// elastic collision between two equal-mass balls: push apart + swap normal velocity
static void collide(Ball *a, Ball *b) {
    if (!a->alive || !b->alive) return;
    float dx_ = b->x - a->x, dy_ = b->y - a->y;
    float d = fsqrt(dx_ * dx_ + dy_ * dy_);
    float rr = BALL_R * 2.0f;
    if (d >= rr || d < 0.0001f) return;

    float nx = dx_ / d, ny = dy_ / d;
    // separate the overlap
    float pen = (rr - d) * 0.5f;
    a->x -= nx * pen; a->y -= ny * pen;
    b->x += nx * pen; b->y += ny * pen;

    // velocity components along the normal
    float va = a->vx * nx + a->vy * ny;
    float vb = b->vx * nx + b->vy * ny;
    if (va - vb <= 0) return;   // already separating
    // equal mass: swap the normal components
    float diff = va - vb;
    a->vx -= diff * nx; a->vy -= diff * ny;
    b->vx += diff * nx; b->vy += diff * ny;

    if (diff > 25) {
        int v = (int)clamp(diff / 60.0f, 1, 6);
        note(60 + (int)clamp(diff / 40.0f, 0, 12), 5, v);
    }
}

// did this ball roll into a pocket mouth?
static bool check_pocket(Ball *b) {
    for (int i = 0; i < 6; i++)
        if (distance((int)b->x, (int)b->y, PKx[i], PKy[i]) < POCKET_R)
            return true;
    return false;
}

static void step(float sdt) {
    for (int i = 0; i < NBALL; i++) {
        Ball *b = &balls[i];
        if (!b->alive) continue;
        b->x += b->vx * sdt;
        b->y += b->vy * sdt;
        rail_bounce(b);

        if (check_pocket(b)) {
            b->alive = false;
            note(72, 7, 5);
            schedule(70, 60, 7, 4);
            if (i == 0) {
                scratched = true;          // cue ball — foul
                shake(4);
            } else {
                pottedThisShot = true;
                potted[cur]++;
                shake(3);
            }
            continue;
        }
        // friction toward rest (exponential decay via dt)
        float f = 1.0f - FRICTION * sdt;
        if (f < 0) f = 0;
        b->vx *= f; b->vy *= f;
        if (speed_of(b) < STOP_SPD) { b->vx = 0; b->vy = 0; }
    }
    // pairwise collisions
    for (int i = 0; i < NBALL; i++)
        for (int j = i + 1; j < NBALL; j++)
            collide(&balls[i], &balls[j]);
}

static void respot_cue(void) {
    // place the cue back on the head spot, nudging if it would overlap a ball
    float hx = T_X1 + (T_X2 - T_X1) * 0.25f;
    float hy = (T_Y1 + T_Y2) / 2.0f;
    for (int tries = 0; tries < 40; tries++) {
        bool ok = true;
        for (int i = 1; i < NBALL; i++)
            if (balls[i].alive && distance((int)hx, (int)hy, (int)balls[i].x, (int)balls[i].y) < BALL_R * 2 + 1)
                { ok = false; break; }
        if (ok) break;
        hx += BALL_R;   // slide right toward open felt
        if (hx > T_X2 - BALL_R) hx = T_X1 + BALL_R;
    }
    balls[0].x = hx; balls[0].y = hy; balls[0].vx = balls[0].vy = 0;
    balls[0].alive = true;
}

static void shoot(float pwr) {
    float a = angle_to((int)balls[0].x, (int)balls[0].y, mouse_x(), mouse_y());
    float spd = 60.0f + pwr * MAX_POWER;
    balls[0].vx = cos_deg(a) * spd;
    balls[0].vy = sin_deg(a) * spd;
    pottedThisShot = false;
    scratched = false;
    if (firstShot) { sfx(-1); note(40, 9, 6); shake((int)(pwr * 6)); firstShot = false; }
    else            note(52, 9, (int)clamp(pwr * 7, 2, 6));
    gstate = 2;
}

// ---------------------------------------------------------------- update
void update(void) {
    float d = dt();
    if (winFlash > 0) winFlash -= d;

    if (gstate == 3) {
        if (keyp(KEY_SPACE) || key('R') || mouse_pressed(MOUSE_LEFT)) new_game();
        return;
    }

    if (key('R')) { new_game(); return; }

    if (gstate == 0) {            // aiming
        if (mouse_pressed(MOUSE_LEFT)) { gstate = 1; charge = 0; chargeDir = 1; }
    }
    else if (gstate == 1) {       // charging power
        // cancel
        if (mouse_pressed(MOUSE_RIGHT) || keyp(KEY_SPACE)) { gstate = 0; charge = 0; return; }
        charge += chargeDir * 1.5f * d;
        if (charge >= 1) { charge = 1; chargeDir = -1; }
        if (charge <= 0) { charge = 0; chargeDir = 1; }
        if (mouse_released(MOUSE_LEFT)) { shoot(charge); charge = 0; }
    }
    else if (gstate == 2) {       // rolling — fixed substeps so nothing tunnels
        for (int s = 0; s < SUBSTEPS; s++) step(d / SUBSTEPS);

        if (!any_moving()) {
            // settle: park everything
            for (int i = 0; i < NBALL; i++) { balls[i].vx = balls[i].vy = 0; }

            if (object_balls_left() == 0 && !scratched) {
                gstate = 3;
                winFlash = 1.6f;
                int total = potted[0] + potted[1];
                if (total > hiPots) { hiPots = total; save_int("poolhi", hiPots); }
                return;
            }
            if (scratched) {
                respot_cue();
                cur = 1 - cur;            // foul: pass turn
                note(36, 8, 5);
            } else if (!pottedThisShot) {
                cur = 1 - cur;            // dry shot: pass turn
            }
            // potted an object ball and didn't scratch → same player shoots again
            gstate = 0;
        }
    }
}

// ---------------------------------------------------------------- draw
static void draw_pocket(int x, int y) {
    circfill(x, y, POCKET_R, CLR_BLACK);
    circ(x, y, POCKET_R, CLR_BROWNISH_BLACK);
}

static void draw_ball(Ball *b) {
    if (!b->alive) return;
    int x = (int)b->x, y = (int)b->y;
    // drop shadow
    ovalfill(x + 1, y + 2, BALL_R, BALL_R - 1, CLR_DARK_GREEN);
    circfill(x, y, BALL_R, b->col);
    // a brighter cap + highlight pip for a bit of sheen
    if (b->col != CLR_WHITE && b->col != CLR_BLACK)
        circfill(x, y - 1, BALL_R - 2, CLR_WHITE), circfill(x, y, BALL_R - 2, b->col);
    pset(x - 1, y - 1, CLR_WHITE);
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);

    // wooden rails (a frame just outside the felt)
    rectfill(T_X1 - RAIL, T_Y1 - RAIL, (T_X2 - T_X1) + RAIL * 2, (T_Y2 - T_Y1) + RAIL * 2, CLR_BROWN);
    rect(T_X1 - RAIL, T_Y1 - RAIL, (T_X2 - T_X1) + RAIL * 2, (T_Y2 - T_Y1) + RAIL * 2, CLR_DARK_BROWN);

    // felt
    rectfill(T_X1, T_Y1, T_X2 - T_X1, T_Y2 - T_Y1, CLR_DARK_GREEN);
    rect(T_X1 - 1, T_Y1 - 1, (T_X2 - T_X1) + 2, (T_Y2 - T_Y1) + 2, CLR_DARK_RED);

    // head string + spot
    int hs = T_X1 + (int)((T_X2 - T_X1) * 0.25f);
    for (int y = T_Y1 + 2; y < T_Y2; y += 4) pset(hs, y, CLR_MEDIUM_GREEN);

    // pockets
    for (int i = 0; i < 6; i++) draw_pocket(PKx[i], PKy[i]);

    // balls
    for (int i = 1; i < NBALL; i++) draw_ball(&balls[i]);
    draw_ball(&balls[0]);

    // aim guide + power, only while the table is at rest and the cue is on the table
    if ((gstate == 0 || gstate == 1) && balls[0].alive) {
        int cx = (int)balls[0].x, cy = (int)balls[0].y;
        float a = angle_to(cx, cy, mouse_x(), mouse_y());
        // dotted projected line that stops at the first rail
        float sx = (float)cx, sy = (float)cy;
        float ddx = cos_deg(a), ddy = sin_deg(a);
        for (int i = 0; i < 90; i++) {
            sx += ddx * 3.0f; sy += ddy * 3.0f;
            if (sx < T_X1 || sx > T_X2 || sy < T_Y1 || sy > T_Y2) break;
            if (i % 2 == 0) pset((int)sx, (int)sy, CLR_LIGHT_YELLOW);
        }
        // a little ghost target where the guide ends
        circ((int)sx, (int)sy, BALL_R, CLR_LIGHT_GREY);

        // cue stick behind the ball, drawn pulled back proportional to charge
        float pull = 6.0f + (gstate == 1 ? charge * 22.0f : 8.0f);
        int bx1 = cx - (int)(ddx * pull);
        int by1 = cy - (int)(ddy * pull);
        int bx2 = cx - (int)(ddx * (pull + 40));
        int by2 = cy - (int)(ddy * (pull + 40));
        line(bx1, by1, bx2, by2, CLR_BROWN);
        line(bx1, by1 + 1, bx2, by2 + 1, CLR_LIGHT_PEACH);
    }

    // HUD
    rectfill(0, 0, SCREEN_W, T_Y1 - RAIL, CLR_BLACK);
    print(str("P1  %d", potted[0]), 6, 4, cur == 0 ? CLR_WHITE : CLR_DARK_GREY);
    print_right(str("%d  P2", potted[1]), SCREEN_W - 6, 4, cur == 1 ? CLR_WHITE : CLR_DARK_GREY);
    print_centered(str("BALLS  %d", object_balls_left()), 4, CLR_LIGHT_GREY);
    print(str("BEST %d", hiPots), 6, 12, CLR_DARK_GREY);

    // power meter (bottom) while charging
    if (gstate == 1) {
        int pw = 140, px = (SCREEN_W - pw) / 2, py = SCREEN_H - 9;
        print_right("hold / release", px - 6, py, CLR_DARK_GREY);
        bar(px, py, pw, 6, charge, charge > 0.8f ? CLR_RED : CLR_ORANGE, CLR_DARKER_GREY);
    } else if (gstate == 0) {
        print_centered(blink(20) ? str("P%d  -  hold mouse to shoot", cur + 1) : str("P%d", cur + 1),
                       SCREEN_H - 9, CLR_LIGHT_GREY);
    } else if (gstate == 2) {
        print_centered("...", SCREEN_H - 9, CLR_DARK_GREY);
    }

    // win banner
    if (gstate == 3) {
        fade(0.35f);
        int winner = (potted[0] >= potted[1]) ? 0 : 1;
        rectfill(SCREEN_W / 2 - 80, SCREEN_H / 2 - 22, 160, 46, CLR_DARK_GREEN);
        rect(SCREEN_W / 2 - 80, SCREEN_H / 2 - 22, 160, 46, blink(8) ? CLR_YELLOW : CLR_WHITE);
        print_centered(str("PLAYER %d WINS", winner + 1), SCREEN_H / 2 - 12, CLR_WHITE);
        print_centered(str("%d - %d", potted[0], potted[1]), SCREEN_H / 2, CLR_LIGHT_YELLOW);
        print_centered("SPACE to rack again", SCREEN_H / 2 + 12, CLR_LIGHT_GREY);
    }
}
