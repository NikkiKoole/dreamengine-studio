/* de:meta
{
  "title": "18. particles",
  "status": "active",
  "created": "2026-05-30",
  "kind": [
    "tutorial",
    "probe"
  ],
  "teaches": [
    "particle-system"
  ],
  "lineage": "Classic gamedev particle-pool tutorial (fountain / explosion / smoke from the same struct array), positioned as tutorial #18 in the series.",
  "description": "The gamedev workhorse: a pool of tiny structs, each with a position, velocity, and a life that ticks down. Move them, age them, recycle the dead ones — and the SAME system becomes a fountain, an explosion, or a puff of smoke just by changing the spawn numbers. Z = explosion, hold X = smoke."
}
de:meta */
#include "studio.h"

// PARTICLES — the gamedev workhorse for sparks, smoke, fire, explosions.
//
// The whole idea: keep an ARRAY of tiny structs. Each one has a position, a
// velocity, and a LIFE that ticks down. Every frame you move them, age them,
// and recycle the dead ones. Change the spawn numbers and the SAME system
// becomes a fountain, a burst, or a puff of smoke.
//
// Z = explosion     X = smoke      (a fountain runs on its own)

#define MAX 600                     // how many particles can be alive at once

typedef struct {
    float x, y;        // position
    float vx, vy;      // velocity (pixels per frame)
    float grav;        // pulled down (+) or floats up (-) each frame
    float life;        // frames left to live; <= 0 means this slot is free
    int   maxlife;     // starting life, so we can fade from full to empty
    float size;        // radius in pixels at birth
    int   color;
} Particle;

static Particle ps[MAX];

// find a free slot and bring one particle to life
static void spawn(float x, float y, float vx, float vy, float grav,
                  int life, float size, int color) {
    for (int i = 0; i < MAX; i++) {
        if (ps[i].life <= 0) {
            ps[i] = (Particle){ x, y, vx, vy, grav, (float)life, life, size, color };
            return;
        }
    }
    // no free slot? we just skip it — the pool is full this frame.
}

void init(void) {
    for (int i = 0; i < MAX; i++) ps[i].life = 0;   // start with everything dead
}

void update(void) {
    // --- the one rule that drives every effect ---
    for (int i = 0; i < MAX; i++) {
        if (ps[i].life <= 0) continue;     // skip dead slots
        ps[i].vy += ps[i].grav;            // gravity bends the path
        ps[i].x  += ps[i].vx;              // move by velocity
        ps[i].y  += ps[i].vy;
        ps[i].life -= 1;                   // age — when it hits 0 the slot frees up
    }

    // --- FOUNTAIN: a few new particles every frame, sprayed upward ---
    // small sideways spread, a strong push up, gravity pulls them back down.
    for (int n = 0; n < 3; n++) {
        int warm[] = { CLR_YELLOW, CLR_ORANGE, CLR_RED, CLR_LIGHT_YELLOW };
        spawn(SCREEN_W / 2, SCREEN_H - 24,
              rnd_float_between(-1.0f, 1.0f),     // vx: gentle spread
              rnd_float_between(-4.5f, -3.0f),    // vy: shoot up
              0.13f,                              // grav: arc back down
              rnd_between(40, 70), 2, warm[rnd(4)]);
    }

    // --- EXPLOSION: one big radial burst on a key press ---
    // pick a random direction for each, push out fast, almost no gravity.
    bool boom = btnp(0, BTN_A) || btnp(1, BTN_A);
    if (boom || frame() == 1) {            // frame()==1 = a welcome burst on load
        for (int n = 0; n < 90; n++) {
            float ang = rnd(360), spd = rnd_float_between(1.5f, 5.0f);
            int hot[] = { CLR_WHITE, CLR_YELLOW, CLR_ORANGE, CLR_RED };
            spawn(SCREEN_W / 2, SCREEN_H / 2,
                  cos_deg(ang) * spd, sin_deg(ang) * spd,
                  0.04f, rnd_between(25, 50), 2, hot[rnd(4)]);
        }
        if (boom) { hit(38, INSTR_NOISE, 5, 180); note(48, INSTR_SQUARE, 3); }   // boom
    }

    // --- SMOKE: slow, rises, gentle drift (negative gravity floats it up) ---
    if (btn(0, BTN_B) || btn(1, BTN_B)) {
        int grey[] = { CLR_LIGHT_GREY, CLR_MEDIUM_GREY, CLR_DARK_GREY, CLR_INDIGO };
        spawn(SCREEN_W / 2 + rnd(20) - 10, SCREEN_H - 24,
              rnd_float_between(-0.4f, 0.4f),
              rnd_float_between(-1.4f, -0.6f),
              -0.015f, rnd_between(60, 100), 3, grey[rnd(4)]);
    }
}

void draw(void) {
    cls(CLR_DARKER_BLUE);

    int alive = 0;
    for (int i = 0; i < MAX; i++) {
        if (ps[i].life <= 0) continue;
        alive++;

        // FADE: shrink as life runs out. life/maxlife goes 1.0 -> 0.0
        float t = ps[i].life / ps[i].maxlife;
        int   r = (int)(ps[i].size * t);

        if (r <= 0) pset((int)ps[i].x, (int)ps[i].y, ps[i].color);
        else        circfill((int)ps[i].x, (int)ps[i].y, r, ps[i].color);
    }

    // labels
    print("PARTICLES", 8, 8, CLR_WHITE);
    print("a pool of structs: move, age, recycle", 8, 20, CLR_LIGHT_GREY);
    print(str("alive: %d / %d", alive, MAX), 8, 32, CLR_MEDIUM_GREY);

    print("Z = explosion", 8, SCREEN_H - 12, CLR_YELLOW);
    print_right("hold X = smoke", SCREEN_W - 8, SCREEN_H - 12, CLR_LIGHT_GREY);
}
