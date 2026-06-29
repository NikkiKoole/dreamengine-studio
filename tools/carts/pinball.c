/* de:meta
{
  "title": "pinball",
  "status": "active",
  "kind": [
    "game"
  ],
  "teaches": [
    "verlet-integration",
    "spring-damper",
    "particle-system"
  ],
  "lineage": "Original hand-built table; the flipper surface-velocity transfer (reflect relative to moving surface) and multi-substep segment/circle collision resolution are the teachable physics core.",
  "genre": "tabletop",
  "description": "A hand-built physical pinball table — gravity, a steel ball that reflects off line-segment walls, swinging flippers with real tip-snap, pop bumpers, slingshots, a DREAM drop-target bank that arms MULTIBALL, a spinner and an orbit multiplier. Z/←/A = left flipper, X/→/D = right, hold ↓ or SPACE to charge the plunger then release to launch, ↑ to nudge (careful — TILT!), SPACE to play/retry."
}
de:meta */
// PINBALL — one hand-built table. The ball is a vector that reflects off
// line-segment walls + circle bumpers; the feel comes from honest physics.
// Plunger launch, two swinging flippers (tip velocity gives the snap),
// pop bumpers, slingshots, a DREAM drop-target bank -> MULTIBALL, a spinner
// for points, a left orbit that bumps the multiplier, nudge-or-TILT, 3 balls.
//
// Compiled at 240x320 (portrait) — see pinball.cart.js.

#include <stddef.h>
#include "studio.h"

// ---------------------------------------------------------------- tuning
#define BALL_R       4
#define GRAV         520.0f
#define MAXSPD       760.0f
#define REST_WALL    0.50f
#define REST_SLING   0.45f
#define SLING_KICK   190.0f
#define BUMP_KICK    170.0f
#define FLIP_E       0.40f
#define FLIP_R       4
#define FLIP_LEN     42
#define FLIP_SPEED   1000.0f   // deg / sec
#define SUBSTEPS     5
#define NPART        80
#define MAXSEG       64
#define DEG2RAD      0.0174532925f

enum { K_WALL = 0, K_SLING = 1 };

typedef struct { float x1, y1, x2, y2; int kind; float lit; } Seg;
typedef struct { int x, y, r; float lit; } Bumper;
typedef struct { float x, y, vx, vy; bool alive; } Ball;
typedef struct { float px, py, ang, rest, up, vang; } Flipper;
typedef struct { float x, y, vx, vy, grav, life, maxlife, size; int col; } Part;

// ---------------------------------------------------------------- state
static int   gstate;            // 0 title, 1 launch, 2 play, 3 over
static int   score, hiscore, ballnum, mult;
static int   letters;           // DREAM bitmask (5 bits)
static int   ballsInPlay;
static float charge;
static bool  prevPlunger;
static float tiltMeter;
static bool  tilted;
static float drainFlash, multiballFlash, tiltFlash, orbitCD;
static float leftKickCD, rightKickCD, leftKickFlash, rightKickFlash;
static float spinnerAngle, spinnerSpin;
static int   chordIdx;

static Seg     segs[MAXSEG];
static int     nseg;
static Bumper  bumps[3];
static Ball    balls[3];
static Flipper fL, fR;
static bool    targUp[5];
static Part    parts[NPART];

static const char DREAM[6] = "DREAM";
static const int  TARG_X[5] = { 50, 76, 102, 128, 154 };
#define TARG_Y 118
#define TARG_W 18
#define TARG_H 8

// spinner zone + orbit gate (left lane)
#define SPIN_X1 16
#define SPIN_X2 46
#define SPIN_Y1 150
#define SPIN_Y2 184
#define ORBIT_Y  148
#define ORBIT_X1 12
#define ORBIT_X2 48

static const int   CHORD_ROOTS[4] = { 60, 57, 53, 55 };          // C  A  F  G
static const int   CHORD_TYPES[4] = { CHORD_MAJ7, CHORD_MIN7, CHORD_MAJ7, CHORD_DOM7 };

// ---------------------------------------------------------------- helpers
static float min4(float a, float b, float c, float d) {
    float m = a; if (b < m) m = b; if (c < m) m = c; if (d < m) m = d; return m;
}

static void addseg(float x1, float y1, float x2, float y2, int kind) {
    if (nseg >= MAXSEG) return;
    segs[nseg].x1 = x1; segs[nseg].y1 = y1;
    segs[nseg].x2 = x2; segs[nseg].y2 = y2;
    segs[nseg].kind = kind; segs[nseg].lit = 0;
    nseg++;
}

static void spawn_part(float x, float y, float vx, float vy, float grav,
                       float life, float size, int col) {
    for (int i = 0; i < NPART; i++) {
        if (parts[i].life <= 0) {
            parts[i] = (Part){ x, y, vx, vy, grav, life, life, size, col };
            return;
        }
    }
}

// point-to-segment reflect, with optional outward kick (slingshots)
static bool reflect_seg(Ball *b, float x1, float y1, float x2, float y2,
                        float e, float kick, float *nx_out, float *ny_out) {
    float dx_ = x2 - x1, dy_ = y2 - y1;
    float L2 = dx_ * dx_ + dy_ * dy_;
    float t = 0;
    if (L2 > 0) t = ((b->x - x1) * dx_ + (b->y - y1) * dy_) / L2;
    t = clamp(t, 0, 1);
    float cx = x1 + dx_ * t, cy = y1 + dy_ * t;
    float ndx = b->x - cx, ndy = b->y - cy;
    float d = fsqrt(ndx * ndx + ndy * ndy);
    if (d > BALL_R) return false;
    float nx, ny;
    if (d > 0.0001f) { nx = ndx / d; ny = ndy / d; } else { nx = 0; ny = -1; }
    b->x += nx * (BALL_R - d);
    b->y += ny * (BALL_R - d);
    float vn = b->vx * nx + b->vy * ny;
    if (vn < 0) { b->vx -= (1 + e) * vn * nx; b->vy -= (1 + e) * vn * ny; }
    if (kick > 0) { b->vx += nx * kick; b->vy += ny * kick; }
    if (nx_out) *nx_out = nx;
    if (ny_out) *ny_out = ny;
    return true;
}

static bool reflect_circle(Ball *b, float cx, float cy, float r,
                           float e, float kick) {
    float ndx = b->x - cx, ndy = b->y - cy;
    float d = fsqrt(ndx * ndx + ndy * ndy);
    float rr = r + BALL_R;
    if (d > rr) return false;
    float nx, ny;
    if (d > 0.0001f) { nx = ndx / d; ny = ndy / d; } else { nx = 0; ny = -1; }
    b->x += nx * (rr - d);
    b->y += ny * (rr - d);
    float vn = b->vx * nx + b->vy * ny;
    if (vn < 0) { b->vx -= (1 + e) * vn * nx; b->vy -= (1 + e) * vn * ny; }
    b->vx += nx * kick;
    b->vy += ny * kick;
    return true;
}

// flipper = pivot->tip segment that swings; transfers tip velocity (the snap)
static bool reflect_flipper(Ball *b, Flipper *f, float e) {
    float tipx = f->px + cos_deg(f->ang) * FLIP_LEN;
    float tipy = f->py + sin_deg(f->ang) * FLIP_LEN;
    float dx_ = tipx - f->px, dy_ = tipy - f->py;
    float L2 = dx_ * dx_ + dy_ * dy_;
    float t = 0;
    if (L2 > 0) t = ((b->x - f->px) * dx_ + (b->y - f->py) * dy_) / L2;
    t = clamp(t, 0, 1);
    float cx = f->px + dx_ * t, cy = f->py + dy_ * t;
    float ndx = b->x - cx, ndy = b->y - cy;
    float d = fsqrt(ndx * ndx + ndy * ndy);
    float rr = BALL_R + FLIP_R;
    if (d > rr) return false;
    float nx, ny;
    if (d > 0.0001f) { nx = ndx / d; ny = ndy / d; } else { nx = 0; ny = -1; }
    b->x += nx * (rr - d);
    b->y += ny * (rr - d);
    // velocity of the contact point: omega cross r  (omega in rad/s)
    float arm = FLIP_LEN * t;
    float vrad = f->vang * DEG2RAD;
    float svx = vrad * (-sin_deg(f->ang)) * arm;
    float svy = vrad * ( cos_deg(f->ang)) * arm;
    // reflect the velocity RELATIVE to the moving surface, then add it back
    float rvx = b->vx - svx, rvy = b->vy - svy;
    float vn = rvx * nx + rvy * ny;
    if (vn < 0) { rvx -= (1 + e) * vn * nx; rvy -= (1 + e) * vn * ny; }
    b->vx = rvx + svx;
    b->vy = rvy + svy;
    return true;
}

static void flipper_update(Flipper *f, bool held, float sdt) {
    float target = held ? f->up : f->rest;
    float prev = f->ang;
    float step = FLIP_SPEED * sdt;
    float diff = target - f->ang;
    if (diff >  step) diff =  step;
    if (diff < -step) diff = -step;
    f->ang += diff;
    f->vang = (sdt > 0) ? (f->ang - prev) / sdt : 0;
}

// ---------------------------------------------------------------- game flow
static void start_launch(void) {
    balls[0] = (Ball){ 219, 290, 0, 0, true };
    balls[1].alive = false;
    balls[2].alive = false;
    ballsInPlay = 1;
    charge = 0; prevPlunger = false;
    tiltMeter = 0; tilted = false;
    gstate = 1;
}

static void new_game(void) {
    score = 0; mult = 1; letters = 0; ballnum = 1;
    for (int i = 0; i < 5; i++) targUp[i] = true;
    for (int i = 0; i < NPART; i++) parts[i].life = 0;
    start_launch();
}

static void do_multiball(void) {
    if (!balls[1].alive) { balls[1] = (Ball){ 90,  200, -40,  60, true }; ballsInPlay++; }
    if (!balls[2].alive) { balls[2] = (Ball){ 150, 200,  40,  60, true }; ballsInPlay++; }
    if (mult < 5) mult++;
    multiballFlash = 1.6f;
    score += 2000;
    for (int i = 0; i < 5; i++) targUp[i] = true;   // re-arm the bank
    letters = 0;
    note(60, 10, 6);
    schedule(120, 67, 10, 6);
    schedule(240, 72, 10, 6);
    shake(6);
}

// ---------------------------------------------------------------- one ball
static void step_ball(Ball *b, float sdt, bool phys) {
    if (!b->alive) return;
    if (phys) b->vy += GRAV * sdt;

    float sp = fsqrt(b->vx * b->vx + b->vy * b->vy);
    if (sp > MAXSPD) { b->vx = b->vx / sp * MAXSPD; b->vy = b->vy / sp * MAXSPD; }

    float prevy = b->y;
    b->x += b->vx * sdt;
    b->y += b->vy * sdt;

    // walls + slingshots
    for (int i = 0; i < nseg; i++) {
        float e    = segs[i].kind == K_SLING ? REST_SLING : REST_WALL;
        float kick = segs[i].kind == K_SLING ? SLING_KICK : 0;
        float nx, ny;
        if (reflect_seg(b, segs[i].x1, segs[i].y1, segs[i].x2, segs[i].y2, e, kick, &nx, &ny)) {
            segs[i].lit = 1.0f;
            if (segs[i].kind == K_SLING) {
                score += 50 * mult;
                note(degree(SCALE_PENTA, 5, rnd(4)), 7, 4);
                shake(2);
                for (int p = 0; p < 6; p++)
                    spawn_part(b->x, b->y, nx * 110 + rnd_float_between(-60, 60),
                               ny * 110 + rnd_float_between(-60, 60), 300, 0.4f, 2, CLR_YELLOW);
            }
        }
    }

    // pop bumpers
    for (int i = 0; i < 3; i++) {
        if (reflect_circle(b, bumps[i].x, bumps[i].y, bumps[i].r, 0.5f, BUMP_KICK)) {
            bumps[i].lit = 1.0f;
            score += 100 * mult;
            shake(3);
            note(48 + i * 5, 5, 5);   // ding, pitch per bumper
            for (int p = 0; p < 6; p++)
                spawn_part(b->x, b->y, rnd_float_between(-120, 120),
                           rnd_float_between(-120, 120), 300, 0.35f, 2, CLR_WHITE);
        }
    }

    // flippers (still solid when tilted — they just sit at rest)
    reflect_flipper(b, &fL, FLIP_E);
    reflect_flipper(b, &fR, FLIP_E);

    // drop targets (AABB, resolve along least penetration)
    for (int i = 0; i < 5; i++) {
        if (!targUp[i]) continue;
        int tx = TARG_X[i], ty = TARG_Y;
        if (b->x + BALL_R > tx && b->x - BALL_R < tx + TARG_W &&
            b->y + BALL_R > ty && b->y - BALL_R < ty + TARG_H) {
            float pl = (b->x + BALL_R) - tx;
            float pr = (tx + TARG_W) - (b->x - BALL_R);
            float pt = (b->y + BALL_R) - ty;
            float pb = (ty + TARG_H) - (b->y - BALL_R);
            float m = min4(pl, pr, pt, pb);
            if      (m == pl) { b->x = tx - BALL_R;         if (b->vx > 0) b->vx = -b->vx * 0.6f; }
            else if (m == pr) { b->x = tx + TARG_W + BALL_R; if (b->vx < 0) b->vx = -b->vx * 0.6f; }
            else if (m == pt) { b->y = ty - BALL_R;         if (b->vy > 0) b->vy = -b->vy * 0.6f; }
            else              { b->y = ty + TARG_H + BALL_R; if (b->vy < 0) b->vy = -b->vy * 0.6f; }
            targUp[i] = false;
            letters |= (1 << i);
            score += 250 * mult;
            shake(2);
            note(degree(SCALE_MAJOR, 5, i), 9, 5);  // rising chime as letters light
            for (int p = 0; p < 5; p++)
                spawn_part(b->x, b->y, rnd_float_between(-80, 80),
                           rnd_float_between(-120, 0), 300, 0.4f, 2, CLR_ORANGE);
            if (letters == 0x1F) do_multiball();
        }
    }

    // spinner
    if (b->x > SPIN_X1 && b->x < SPIN_X2 && b->y > SPIN_Y1 && b->y < SPIN_Y2) {
        float s2 = fsqrt(b->vx * b->vx + b->vy * b->vy);
        spinnerSpin = s2;
        spinnerAngle += s2 * sdt * 1.4f;
        score += mult;
        if (s2 > 80 && frame() % 3 == 0) hit(degree(SCALE_PENTA, 5, rnd(5)), 11, 2, 40);
    }

    // left orbit gate — bump the multiplier when looped upward
    if (prevy > ORBIT_Y && b->y <= ORBIT_Y &&
        b->x > ORBIT_X1 && b->x < ORBIT_X2 && orbitCD <= 0) {
        if (mult < 5) mult++;
        orbitCD = 1.5f;
        for (int k = 0; k < 4; k++) schedule(k * 55, degree(SCALE_PENTA, 4, k * 2), 9, 4);
    }

    // ---- outlane kickbacks: a ball collected in a side pocket is fired back
    // up the playfield instead of draining (the "reshoot"). Always available,
    // with a short cooldown so it can't double-fire on the same ball.
    if (b->x < 32 && b->y > 296 && leftKickCD <= 0) {
        b->vx = 135; b->vy = -400;
        leftKickCD = 0.4f; leftKickFlash = 0.5f;
        score += 50 * mult;
        shake(4);
        note(36, 10, 6);                       // solenoid thunk
        for (int p = 0; p < 8; p++)
            spawn_part(b->x, b->y, rnd_float_between(-40, 90),
                       rnd_float_between(-200, -60), 300, 0.45f, 2, CLR_LIME_GREEN);
    }
    // (x < 207 keeps this off the plunger lane, which sits right of the divider)
    if (b->x > 186 && b->x < 207 && b->y > 296 && rightKickCD <= 0) {
        b->vx = -135; b->vy = -400;
        rightKickCD = 0.4f; rightKickFlash = 0.5f;
        score += 50 * mult;
        shake(4);
        note(36, 10, 6);
        for (int p = 0; p < 8; p++)
            spawn_part(b->x, b->y, rnd_float_between(-90, 40),
                       rnd_float_between(-200, -60), 300, 0.45f, 2, CLR_LIME_GREEN);
    }

    // drain
    if (b->y > 318) {
        b->alive = false;
        ballsInPlay--;
        drainFlash = 0.6f;
        shake(4);
        note(43, 12, 5);
        schedule(140, 36, 12, 5);
    }
}

// ---------------------------------------------------------------- init
void init(void) {
    bpm(96);

    // instruments (slots 5..15)
    instrument(5,  INSTR_SINE,   2,  60, 0,  90);              // bumper ding
    instrument(6,  INSTR_NOISE,  1,  30, 0,  40);              // flipper clack
    instrument(7,  INSTR_SQUARE, 2,  50, 0,  70);              // slingshot twang
    instrument_duty(7, 0.25f);
    instrument(8,  INSTR_TRI,   12, 140, 4, 220);              // lounge chords
    instrument(9,  INSTR_SINE,   3,  60, 2, 130);              // sparkle / letters
    instrument(10, INSTR_SQUARE, 4,  40, 6, 220);              // multiball klaxon
    instrument_lfo(10, 0, LFO_PITCH, 8.0f, 2.0f);
    instrument(11, INSTR_SAW,    3,  30, 5,  70);              // spinner whir
    instrument_lfo(11, 0, LFO_PITCH, 11.0f, 0.6f);
    instrument(12, INSTR_SINE,   4, 120, 3, 240);              // drain (sad)

    hiscore = load_int("pinhi", 0);

    // ---- build the table geometry ----
    nseg = 0;
    // top dome: arc centered (120,146) r110, from 180deg to 360deg
    {
        int N = 18;
        float px = 0, py = 0;
        for (int i = 0; i <= N; i++) {
            float a = 180.0f + 180.0f * (float)i / (float)N;
            float x = 120 + cos_deg(a) * 110.0f;
            float y = 146 + sin_deg(a) * 110.0f;
            if (i > 0) addseg(px, py, x, y, K_WALL);
            px = x; py = y;
        }
    }
    // outer walls down from the dome ends (10,146) and (230,146)
    addseg(10, 146, 10, 314, K_WALL);     // left (down into the kickback pocket)
    addseg(230, 146, 230, 304, K_WALL);   // right (outer edge of plunger lane)
    addseg(208, 108, 208, 314, K_WALL);   // plunger-lane divider (gap above y108)
    // bottom: each side funnels into its OWN narrow kickback pocket (a corner
    // against the outer wall); the only true drain is the center gap between the
    // flipper tips. A ball that slips past a flipper rolls into the pocket and
    // the kicker fires it back up the playfield -- the "reshoot", see step_ball().
    addseg(60, 300, 12, 314, K_WALL);     // left funnel floor -> bottom-left pocket
    addseg(158, 300, 206, 314, K_WALL);   // right funnel floor -> bottom-right pocket
    // slingshots (kick the ball back into play)
    addseg(40, 248, 58, 286, K_SLING);    // left
    addseg(168, 248, 150, 286, K_SLING);  // right

    // bumpers
    bumps[0] = (Bumper){ 58, 182, 12, 0 };
    bumps[1] = (Bumper){ 162, 182, 12, 0 };
    bumps[2] = (Bumper){ 110, 214, 12, 0 };

    // flippers
    fL = (Flipper){ 60, 283,  26,  26, -24, 0 };
    fR = (Flipper){ 158, 283, 154, 154, 204, 0 };

    gstate = 0;  // title
}

// ---------------------------------------------------------------- update
void update(void) {
    float d = dt();

    // decay timers / lit flashes
    if (drainFlash > 0)     drainFlash    -= d;
    if (multiballFlash > 0) multiballFlash -= d;
    if (tiltFlash > 0)      tiltFlash     -= d;
    if (orbitCD > 0)        orbitCD       -= d;
    if (leftKickCD > 0)     leftKickCD    -= d;
    if (rightKickCD > 0)    rightKickCD   -= d;
    if (leftKickFlash > 0)  leftKickFlash -= d;
    if (rightKickFlash > 0) rightKickFlash -= d;
    spinnerSpin *= 0.92f;
    if (tiltMeter > 0) { tiltMeter -= d * 0.6f; if (tiltMeter < 0) tiltMeter = 0; }
    for (int i = 0; i < nseg; i++) if (segs[i].lit > 0) segs[i].lit -= d * 3.0f;
    for (int i = 0; i < 3; i++)    if (bumps[i].lit > 0) bumps[i].lit -= d * 3.0f;

    // particles
    for (int i = 0; i < NPART; i++) {
        if (parts[i].life <= 0) continue;
        parts[i].vy   += parts[i].grav * d;
        parts[i].x    += parts[i].vx * d;
        parts[i].y    += parts[i].vy * d;
        parts[i].life -= d;
    }

    // lounge bed (always running)
    if (every(4)) { chord(CHORD_ROOTS[chordIdx], CHORD_TYPES[chordIdx], 8, 2); chordIdx = (chordIdx + 1) % 4; }
    if (every(2) && chance(60)) note(degree(SCALE_MAJOR, 5, rnd(5)), 9, 1);

    if (gstate == 0) {  // title
        if (keyp(KEY_SPACE) || btnp(0, BTN_A) || btnp(0, BTN_DOWN)) new_game();
        return;
    }
    if (gstate == 3) {  // game over
        if (keyp(KEY_SPACE) || btnp(0, BTN_A)) new_game();
        return;
    }

    // flipper input (Z / A / left  and  X / D / right)
    bool fl = !tilted && (btn(0, BTN_A) || btn(0, BTN_LEFT)  || btn(1, BTN_LEFT)  || key('Z'));
    bool fr = !tilted && (btn(0, BTN_B) || btn(0, BTN_RIGHT) || btn(1, BTN_RIGHT) || key('X'));
    if (!tilted && (btnp(0, BTN_A) || btnp(0, BTN_LEFT)  || btnp(1, BTN_LEFT)))  hit(40, 6, 2, 40);
    if (!tilted && (btnp(0, BTN_B) || btnp(0, BTN_RIGHT) || btnp(1, BTN_RIGHT))) hit(40, 6, 2, 40);

    // nudge (careful — TILT!)
    if (!tilted && (btnp(0, BTN_UP) || btnp(1, BTN_UP))) {
        for (int i = 0; i < 3; i++)
            if (balls[i].alive) { balls[i].vy -= 90; balls[i].vx += rnd_float_between(-50, 50); }
        tiltMeter += 0.4f;
        shake(3);
        if (tiltMeter > 1.0f) { tilted = true; tiltFlash = 1.5f; note(30, 10, 6); shake(8); }
    }

    // plunger
    if (gstate == 1) {
        bool pl = btn(0, BTN_DOWN) || btn(1, BTN_DOWN) || key(KEY_SPACE);
        if (pl) charge = clamp(charge + d * 1.4f, 0, 1);
        else if (prevPlunger && charge > 0.05f) {
            balls[0].vy = -(200 + charge * 760);
            balls[0].vx = -charge * 30;
            charge = 0;
            gstate = 2;
            ballsInPlay = 1;
            hit(52, 7, 4, 90);
        }
        prevPlunger = pl;
        // pin the ball in the plunger lane WHILE charging — but not on the
        // frame we launch (the launch branch above flips gstate to 2, so this
        // re-check skips the reset that would otherwise zero the kick velocity)
        if (gstate == 1) { balls[0].x = 219; balls[0].y = 290; balls[0].vx = 0; balls[0].vy = 0; }
    }

    // physics substeps
    bool phys = (gstate == 2);
    for (int s = 0; s < SUBSTEPS; s++) {
        float sdt = d / SUBSTEPS;
        flipper_update(&fL, fl, sdt);
        flipper_update(&fR, fr, sdt);
        for (int i = 0; i < 3; i++) step_ball(&balls[i], sdt, phys && balls[i].alive);
    }

    // all balls drained?
    if (gstate == 2 && ballsInPlay <= 0) {
        if (ballnum >= 3) {
            gstate = 3;
            if (score > hiscore) { hiscore = score; save_int("pinhi", hiscore); }
        } else {
            ballnum++;
            start_launch();
        }
    }
}

// ---------------------------------------------------------------- draw
static void draw_flipper(Flipper *f, int col) {
    float tx = f->px + cos_deg(f->ang) * FLIP_LEN;
    float ty = f->py + sin_deg(f->ang) * FLIP_LEN;
    for (int i = 0; i <= 10; i++) {
        float t = (float)i / 10.0f;
        circfill((int)lerp(f->px, tx, t), (int)lerp(f->py, ty, t), FLIP_R, col);
    }
    circfill((int)f->px, (int)f->py, FLIP_R + 1, CLR_DARK_GREY);
}

static void draw_ball(Ball *b) {
    if (!b->alive) return;
    // cheap motion blur
    for (int k = 1; k <= 3; k++)
        circfill((int)(b->x - b->vx * 0.006f * k), (int)(b->y - b->vy * 0.006f * k),
                 BALL_R - 1, CLR_DARK_GREY);
    circfill((int)b->x, (int)b->y, BALL_R, CLR_LIGHT_GREY);
    circfill((int)b->x + 1, (int)b->y + 1, BALL_R - 2, CLR_MEDIUM_GREY);
    pset((int)b->x - 1, (int)b->y - 1, CLR_WHITE);
}

void draw(void) {
    cls(CLR_DARKER_BLUE);

    // playfield walls
    for (int i = 0; i < nseg; i++) {
        int col = (segs[i].kind == K_SLING) ? CLR_ORANGE : CLR_TRUE_BLUE;
        if (segs[i].lit > 0.3f) col = CLR_WHITE;
        line((int)segs[i].x1, (int)segs[i].y1, (int)segs[i].x2, (int)segs[i].y2, col);
        if (segs[i].kind == K_SLING)
            line((int)segs[i].x1 + 1, (int)segs[i].y1, (int)segs[i].x2 + 1, (int)segs[i].y2, col);
    }

    // spinner
    {
        int cx = (SPIN_X1 + SPIN_X2) / 2, cy = (SPIN_Y1 + SPIN_Y2) / 2;
        circ(cx, cy, 11, CLR_DARK_GREY);
        int bx = (int)(cos_deg(spinnerAngle) * 10), by = (int)(sin_deg(spinnerAngle) * 10);
        line(cx + bx, cy + by, cx - bx, cy - by, spinnerSpin > 60 ? CLR_YELLOW : CLR_LIGHT_GREY);
    }

    // orbit chevron
    line(ORBIT_X1 + 6, ORBIT_Y + 6, (ORBIT_X1 + ORBIT_X2) / 2, ORBIT_Y, CLR_LIME_GREEN);
    line(ORBIT_X2 - 6, ORBIT_Y + 6, (ORBIT_X1 + ORBIT_X2) / 2, ORBIT_Y, CLR_LIME_GREEN);

    // outlane kickers (the reshoot) — an up-arrow in each corner pocket;
    // glows lime, flashes white the moment it fires the ball back up
    {
        int lc = leftKickFlash  > 0 ? CLR_WHITE : CLR_LIME_GREEN;
        int rc = rightKickFlash > 0 ? CLR_WHITE : CLR_LIME_GREEN;
        line(14, 308, 20, 299, lc); line(20, 299, 26, 308, lc); line(20, 299, 20, 311, lc);
        line(194, 308, 200, 299, rc); line(200, 299, 206, 308, rc); line(200, 299, 200, 311, rc);
    }

    // drop targets
    for (int i = 0; i < 5; i++) {
        int tx = TARG_X[i], ty = TARG_Y;
        if (targUp[i]) {
            rectfill(tx, ty, TARG_W, TARG_H, CLR_RED);
            rect(tx, ty, TARG_W, TARG_H, CLR_WHITE);
            char s[2] = { DREAM[i], 0 };
            print(s, tx + TARG_W / 2 - 3, ty + 1, CLR_WHITE);
        } else {
            rectfill(tx, ty + TARG_H - 2, TARG_W, 2, CLR_DARK_RED);
        }
    }

    // bumpers
    for (int i = 0; i < 3; i++) {
        int x = bumps[i].x, y = bumps[i].y, r = bumps[i].r;
        if (bumps[i].lit > 0.2f) {
            circfill(x, y, r + 1, CLR_WHITE);
        } else {
            circfill(x, y, r, CLR_DARK_PURPLE);
            circfill(x, y, r - 3, CLR_PINK);
            circ(x, y, r, CLR_MAUVE);
        }
    }

    // particles
    for (int i = 0; i < NPART; i++) {
        if (parts[i].life <= 0) continue;
        float t = parts[i].life / parts[i].maxlife;
        int r = (int)(parts[i].size * t);
        if (r < 1) pset((int)parts[i].x, (int)parts[i].y, parts[i].col);
        else       circfill((int)parts[i].x, (int)parts[i].y, r, parts[i].col);
    }

    // flippers + balls
    draw_flipper(&fL, tilted ? CLR_DARK_GREY : CLR_LIGHT_GREY);
    draw_flipper(&fR, tilted ? CLR_DARK_GREY : CLR_LIGHT_GREY);
    for (int i = 0; i < 3; i++) draw_ball(&balls[i]);

    // plunger charge meter (right margin)
    if (gstate == 1) {
        int h = (int)(charge * 120);
        rect(232, 178, 7, 124, CLR_DARK_GREY);
        rectfill(233, 300 - h, 5, h, charge > 0.8f ? CLR_RED : CLR_GREEN);
    }

    // HUD strip
    rectfill(0, 0, SCREEN_W, 30, CLR_BLACK);
    line(0, 30, SCREEN_W, 30, CLR_DARK_GREY);
    print(str("%d", score), 4, 4, CLR_YELLOW);
    print_right(str("HI %d", hiscore), 236, 4, CLR_LIGHT_GREY);
    for (int i = 0; i < 5; i++) {
        char s[2] = { DREAM[i], 0 };
        print(s, 92 + i * 12, 4, (letters & (1 << i)) ? CLR_YELLOW : CLR_DARK_GREY);
    }
    print(str("BALL %d/3   x%d", ballnum, mult), 4, 16, CLR_LIGHT_GREY);

    // overlays
    if (tilted)
        print_centered("T I L T", SCREEN_W/2, SCREEN_H / 2 - 4, blink(4) ? CLR_RED : CLR_DARK_RED);
    if (multiballFlash > 0)
        print_centered("MULTIBALL!", SCREEN_W/2, 60, blink(4) ? CLR_YELLOW : CLR_ORANGE);
    if (drainFlash > 0) {
        rect(0, 0, SCREEN_W, SCREEN_H, CLR_RED);
        rect(1, 1, SCREEN_W - 2, SCREEN_H - 2, CLR_DARK_RED);
    }

    if (gstate == 0) {  // title
        rectfill(24, 120, 192, 90, CLR_DARK_BLUE);
        rect(24, 120, 192, 90, CLR_YELLOW);
        int w = text_width("PINBALL") * 3;
        print_scaled("PINBALL", (SCREEN_W - w) / 2, 134, CLR_WHITE, 3);
        print_centered("a hand-built table", SCREEN_W/2, 166, CLR_LIGHT_GREY);
        if (blink(20)) print_centered("SPACE  TO  PLAY", SCREEN_W/2, 188, CLR_GREEN);
    }
    if (gstate == 3) {  // game over
        rectfill(24, 120, 192, 90, CLR_DARK_BLUE);
        rect(24, 120, 192, 90, CLR_RED);
        int w = text_width("GAME OVER") * 2;
        print_scaled("GAME OVER", (SCREEN_W - w) / 2, 132, CLR_RED, 2);
        print_centered(str("SCORE  %d", score), SCREEN_W/2, 158, CLR_YELLOW);
        print_centered(str("BEST   %d", hiscore), SCREEN_W/2, 170, CLR_LIGHT_GREY);
        if (blink(20)) print_centered("SPACE  TO  RETRY", SCREEN_W/2, 190, CLR_GREEN);
    }
}
