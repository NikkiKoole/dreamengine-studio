/* de:meta
{
  "title": "Rollswarm",
  "status": "active",
  "created": "2026-06-01",
  "kind": [
    "game"
  ],
  "teaches": [
    "flocking",
    "camera-follow"
  ],
  "lineage": "Descends from katamari (same grow-by-eating loop, same world-bigger-than-screen pan) but replaces static objects with a live boid flock whose four-urge steering (separation / alignment / cohesion / flee) is the conceptual core.",
  "genre": "sandbox",
  "description": "Katamari x boids: roll a growing ball through a panicking flock that bolts harder the closer you get, swallowing any critter smaller than you to snowball toward the size goal before time runs out. WASD/arrows to roll, Z to replay.",
  "todo": [
    "Touch: needs an onscreen joystick.",
    "Feels a bit random — vary enemy speed to help."
  ]
}
de:meta */
#include "studio.h"

// ROLLSWARM — Katamari × Boids. Roll a growing ball through a panicking flock.
//
// The arena is full of a single BOID FLOCK: each critter obeys the three flocking
// urges (separation / alignment / cohesion) AND a fourth, overriding one — FLEE the
// ball, harder the closer it gets. So the herd boils away from you, scatters in
// terror, then re-flocks the moment you pass. You can only swallow a critter once
// your radius beats its size; every meal grows the ball (and its reach), so the
// flock bolts from ever farther as you snowball. Hit the SIZE GOAL before the clock
// runs out.
//
//   WASD / arrows : roll the ball
//   Z             : replay when time's up

#define WORLDW   560
#define WORLDH   360
#define N        130          // flock size
#define VIEW     22.0f        // neighbour radius for flocking
#define SEP2     90.0f        // squared distance below which we push apart
#define FLEE_R   90.0f        // how far a critter can sense the ball
#define MAXSPD   1.9f         // normal cruising speed cap
#define PANICSPD 3.6f         // speed cap while fleeing
#define GOAL     34           // win when ball radius reaches this
#define TIME_S   60

typedef struct {
    float x, y, vx, vy;
    float size;               // body radius — gates whether it can be eaten
    int   col, shade;         // base + darker companion shade
    float panic;              // 0..1, how scared it is right now (drives color/anim)
    bool  alive;
} Critter;

typedef struct { float ang, frac, size; int col; } Stuck;   // bits clinging to the ball

static float px, py, ballr;          // ball centre + radius
static Critter herd[N];
static Stuck   stuck[64];
static int     nstuck;
static int     eaten;
static float   t_end;
static bool    over, won;

static const int PALS[]   = { CLR_GREEN, CLR_BLUE, CLR_PINK, CLR_YELLOW, CLR_ORANGE,
                              CLR_LIME_GREEN, CLR_PEACH, CLR_LIGHT_PEACH };
static const int SHADES[] = { CLR_MEDIUM_GREEN, CLR_TRUE_BLUE, CLR_MAUVE, CLR_ORANGE, CLR_DARK_ORANGE,
                              CLR_MEDIUM_GREEN, CLR_DARK_PEACH, CLR_DARK_PEACH };

static void spawn_flock(void) {
    for (int i = 0; i < N; i++) {
        int tier = rnd(10);
        // a spread of sizes so the size-gate keeps mattering as you grow:
        // mostly tiny, some medium, a few chunky.
        float sz = tier < 6 ? rnd_float_between(2.0f, 4.0f)
                 : tier < 9 ? rnd_float_between(5.0f, 9.0f)
                            : rnd_float_between(11.0f, 18.0f);
        int ci = rnd((int)(sizeof PALS / sizeof *PALS));
        float a = rnd(360);
        herd[i] = (Critter){
            rnd_between(20, WORLDW - 20), rnd_between(20, WORLDH - 20),
            cos_deg(a) * MAXSPD, sin_deg(a) * MAXSPD,
            sz, PALS[ci], SHADES[ci], 0.0f, true
        };
    }
}

static void step(void) {
    for (int i = 0; i < N; i++) {
        if (!herd[i].alive) continue;
        Critter *b = &herd[i];

        float sepx = 0, sepy = 0, alx = 0, aly = 0, cox = 0, coy = 0;
        int n = 0;
        for (int j = 0; j < N; j++) {
            if (j == i || !herd[j].alive) continue;
            float dx = herd[j].x - b->x, dy = herd[j].y - b->y;
            float d2 = dx * dx + dy * dy;
            if (d2 < VIEW * VIEW) {
                n++;
                alx += herd[j].vx; aly += herd[j].vy;
                cox += herd[j].x;  coy += herd[j].y;
                if (d2 < SEP2) { sepx -= dx; sepy -= dy; }
            }
        }
        if (n > 0) {
            alx /= n; aly /= n; cox /= n; coy /= n;
            b->vx += (alx - b->vx) * 0.05f + (cox - b->x) * 0.0010f + sepx * 0.020f;
            b->vy += (aly - b->vy) * 0.05f + (coy - b->y) * 0.0010f + sepy * 0.020f;
        }

        // FLEE the ball — the dominant urge. Strength scales hard with closeness, and
        // grows with the ball's reach so a big ball terrifies from farther away.
        float pdx = b->x - px, pdy = b->y - py;
        float pd = fsqrt(pdx * pdx + pdy * pdy);
        float sense = FLEE_R + ballr;          // bigger ball = wider terror radius
        if (pd < sense && pd > 0.01f) {
            float t = 1.0f - pd / sense;        // 0 at the edge → 1 right on top
            b->panic = t > b->panic ? t : b->panic;
            float push = 0.6f + t * t * 5.5f;   // ramps up sharply when close
            b->vx += pdx / pd * push;
            b->vy += pdy / pd * push;
        }
        b->panic *= 0.90f;                      // calm down over time → re-flock

        // speed cap rises while panicking (adrenaline)
        float cap = MAXSPD + b->panic * (PANICSPD - MAXSPD);
        float sp = fsqrt(b->vx * b->vx + b->vy * b->vy);
        if (sp > cap)            { b->vx = b->vx / sp * cap;  b->vy = b->vy / sp * cap; }
        else if (sp > 0 && sp < 0.6f) { b->vx = b->vx / sp * 0.6f; b->vy = b->vy / sp * 0.6f; }

        b->x += b->vx; b->y += b->vy;

        // bounce off arena walls (a trapped herd panics nicely in the corners)
        if (b->x < 6)            { b->x = 6;            b->vx = -b->vx * 0.6f; }
        if (b->x > WORLDW - 6)   { b->x = WORLDW - 6;   b->vx = -b->vx * 0.6f; }
        if (b->y < 6)            { b->y = 6;            b->vy = -b->vy * 0.6f; }
        if (b->y > WORLDH - 6)   { b->y = WORLDH - 6;   b->vy = -b->vy * 0.6f; }
    }
}

void init(void) {
    px = WORLDW / 2.0f; py = WORLDH / 2.0f; ballr = 5.0f;
    nstuck = eaten = 0; over = won = false;
    spawn_flock();
    for (int i = 0; i < 90; i++) step();     // let the flock settle before play
    t_end = now() + TIME_S;

    // soft, anxious pad under the chase
    instrument(5, INSTR_TRI, 200, 150, 3, 400);
    instrument_filter(5, FILTER_LOW, 700, 5);
    bpm(112);
}

void update(void) {
    if (over) { if (btnp(0, BTN_A) || btnp(1, BTN_A)) init(); return; }

    if (now() >= t_end) {
        over = true;
        won  = (int)ballr >= GOAL;
        sfx(-1);
        return;
    }

    // roll — a bigger ball is a touch slower
    float spd = clamp(3.4f - ballr * 0.020f, 1.5f, 3.4f);
    float mx = (btn(0, BTN_RIGHT) || btn(1, BTN_RIGHT)) - (btn(0, BTN_LEFT) || btn(1, BTN_LEFT));
    float my = (btn(0, BTN_DOWN)  || btn(1, BTN_DOWN))  - (btn(0, BTN_UP)   || btn(1, BTN_UP));
    px = clamp(px + mx * spd, ballr, WORLDW - ballr);
    py = clamp(py + my * spd, ballr, WORLDH - ballr);

    step();

    // absorb: critter must be smaller than the ball AND inside the absorb radius
    for (int i = 0; i < N; i++) {
        if (!herd[i].alive) continue;
        float d = distance((int)px, (int)py, (int)herd[i].x, (int)herd[i].y);
        if (herd[i].size <= ballr * 0.92f && d < ballr + herd[i].size * 0.4f) {
            // grow by area, stick a token on the ball, ping a note
            ballr = fsqrt(ballr * ballr + herd[i].size * herd[i].size * 0.55f);
            if (ballr > GOAL + 8) ballr = GOAL + 8;
            herd[i].alive = false;
            eaten++;
            if (nstuck < 64) {
                stuck[nstuck++] = (Stuck){ rnd(360), rnd_float_between(0.45f, 0.92f),
                                           herd[i].size < 7 ? herd[i].size : 7.0f, herd[i].col };
            }
            note(50 + (eaten % 16), INSTR_SQUARE, 3);
            shake(1.2f);
        }
    }

    if (every(2)) note(36, 5, 2);   // low anxious pulse
}

static void draw_critter(const Critter *b) {
    float sp = fsqrt(b->vx * b->vx + b->vy * b->vy);
    if (sp < 0.01f) sp = 0.01f;
    float nx = b->vx / sp, ny = b->vy / sp;
    float s = b->size;                       // body half-length
    // a little arrowhead pointing along velocity; scared ones flash brighter
    int col = b->panic > 0.55f ? CLR_WHITE : b->col;
    trifill((int)(b->x + nx * (s + 2)),               (int)(b->y + ny * (s + 2)),
            (int)(b->x - nx * s - ny * s * 0.8f),     (int)(b->y - ny * s + nx * s * 0.8f),
            (int)(b->x - nx * s + ny * s * 0.8f),     (int)(b->y - ny * s - nx * s * 0.8f),
            col);
    // a dark belly dot for the bigger ones so size reads at a glance
    if (b->size >= 5.0f) pset((int)b->x, (int)b->y, b->shade);
}

void draw(void) {
    cls(CLR_DARKER_BLUE);
    follow((int)px, (int)py, WORLDW, WORLDH);

    // arena floor grid + border
    for (int gx = 0; gx <= WORLDW; gx += 32) line(gx, 0, gx, WORLDH, CLR_DARKER_GREY);
    for (int gy = 0; gy <= WORLDH; gy += 32) line(0, gy, WORLDW, gy, CLR_DARKER_GREY);
    rect(0, 0, WORLDW, WORLDH, CLR_BLUE_GREEN);

    // a faint ring showing the ball's terror reach (where the flock starts to bolt)
    circ((int)px, (int)py, (int)(FLEE_R + ballr), CLR_DARKER_GREY);

    // the flock
    for (int i = 0; i < N; i++) if (herd[i].alive) draw_critter(&herd[i]);

    // the ball + everything clinging to it
    circfill((int)px, (int)py, (int)ballr, CLR_BROWN);
    circ((int)px, (int)py, (int)ballr, CLR_DARK_BROWN);
    // a little highlight so it reads as a sphere
    circfill((int)(px - ballr * 0.35f), (int)(py - ballr * 0.35f),
             (int)(ballr * 0.28f) + 1, CLR_MEDIUM_GREY);
    for (int i = 0; i < nstuck; i++) {
        int sx = (int)(px + cos_deg(stuck[i].ang) * stuck[i].frac * ballr);
        int sy = (int)(py + sin_deg(stuck[i].ang) * stuck[i].frac * ballr);
        circfill(sx, sy, (int)stuck[i].size, stuck[i].col);
    }

    camera(0, 0);

    // HUD
    int secs = (int)(t_end - now()); if (secs < 0) secs = 0;
    rectfill(0, 0, SCREEN_W, 11, CLR_DARKER_PURPLE);
    print(str("SIZE %d", (int)ballr), 6, 2, CLR_WHITE);
    print(str("/%d", GOAL), 6 + text_width(str("SIZE %d", (int)ballr)) + 1, 2, CLR_LIME_GREEN);
    print_centered(str("ate %d", eaten), SCREEN_W/2, 2, CLR_LIGHT_GREY);
    print_right(str("%d", secs), SCREEN_W - 6, 2, secs <= 10 ? CLR_RED : CLR_WHITE);

    // a slim progress bar toward the goal
    float prog = clamp((float)ballr / GOAL, 0, 1);
    bar(6, 13, SCREEN_W - 12, 3, prog, CLR_LIME_GREEN, CLR_DARKER_GREY);

    if (over) {
        rectfill(60, 70, 200, 56, CLR_BLACK);
        rect(60, 70, 200, 56, won ? CLR_LIME_GREEN : CLR_RED);
        print_centered(won ? "YOU SNOWBALLED!" : "TIME'S UP", SCREEN_W/2, 80, won ? CLR_YELLOW : CLR_RED);
        print_centered(str("final size %d  -  ate %d", (int)ballr, eaten), SCREEN_W/2, 96, CLR_WHITE);
        print_centered("Z to replay", SCREEN_W/2, 110, CLR_LIGHT_GREY);
    }
}
