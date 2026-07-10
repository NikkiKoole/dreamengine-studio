/* de:meta
{
  "slug": "blendfx",
  "title": "blend fx — glowing particles",
  "status": "active",
  "created": "2026-07-10",
  "kind": [
    "tutorial",
    "probe"
  ],
  "teaches": [
    "blend-modes",
    "particle-system"
  ],
  "lineage": "The engine demo for blend() (ADR-0031 / docs/design/blend-tables.md): additive glow was impossible with plain overwrite — this is the effect that motivated the feature. Particle pool copied from the particles.c workhorse.",
  "description": {
    "summary": "A fountain of particles that GLOW: with BLEND_ADD each spark brightens whatever is behind it, and overlapping sparks stack toward white — real light, not colored dots.",
    "detail": "Same particle pool as the particles tutorial, but drawn through blend(). BLEND_ADD = additive glow (fire, magic, lasers, brightens the background); BLEND_AVG = translucent (glass, smoke); BLEND_MUL = darkening + tint (fog, shadow); BLEND_SUB = carve light toward black (cold shadow, scorch). Watch the sparks over the bright band vs over black — the mix depends on what is behind, which the old fillp dither fake can't do.",
    "controls": "1 = ADD   2 = AVG   3 = MUL   4 = SUB   0 = no blend (plain overwrite)   SPACE = burst"
  }
}
de:meta */
#include "studio.h"

// BLEND FX — the particle demo for blend(). Additive glow is the headline: with
// BLEND_ADD, each translucent spark ADDS its light to the pixel behind it, so a
// dense cluster stacks toward white. That "reads as light" quality is exactly what
// plain overwrite (and the fillp dither fake) can't do — the fake ignores the
// background, blend() mixes with it.
//
// 1 = ADD (glow)   2 = AVG (glass/smoke)   3 = MUL (fog)   0 = off   SPACE = burst

#define MAX 500

typedef struct {
    float x, y, vx, vy;
    float life;
    int   maxlife;
    float size;
    int   color;
} Particle;

static Particle ps[MAX];
static int mode = BLEND_ADD;   // 0=off,1=ADD,2=AVG,3=MUL — matches the BLEND_* values

static void spawn(float x, float y, float vx, float vy, int life, float size, int color) {
    for (int i = 0; i < MAX; i++) {
        if (ps[i].life <= 0) {
            ps[i] = (Particle){ x, y, vx, vy, (float)life, life, size, color };
            return;
        }
    }
}

// a small radial burst of warm sparks
static void burst(float x, float y) {
    for (int i = 0; i < 60; i++) {
        float a = (float)i / 60.0f * 360.0f;
        float sp = 0.6f + (i % 7) * 0.25f;
        int warm[3] = { CLR_RED, CLR_ORANGE, CLR_YELLOW };
        spawn(x, y, cos_deg(a) * sp, sin_deg(a) * sp, 40 + i % 30, 3.0f, warm[i % 3]);
    }
}

void init(void) {
    for (int i = 0; i < MAX; i++) ps[i].life = 0;
    // pre-seed a rising column so the very first frame (and the baked thumbnail) already glows
    for (int n = 0; n < 90; n++) {
        int warm[3] = { CLR_RED, CLR_ORANGE, CLR_YELLOW };
        spawn(SCREEN_W / 2.0f + (n % 7 - 3) * 3.0f, SCREEN_H - 10 - n * 1.4f,
              (n % 7 - 3) * 0.15f, -1.8f, 30 + n, 3.5f, warm[n % 3]);
    }
}

void update(void) {
    if (keyp('0')) mode = BLEND_NONE;
    if (keyp('1')) mode = BLEND_ADD;
    if (keyp('2')) mode = BLEND_AVG;
    if (keyp('3')) mode = BLEND_MUL;
    if (keyp('4')) mode = BLEND_SUB;
    if (keyp(KEY_SPACE)) burst(SCREEN_W / 2.0f, SCREEN_H / 2.0f);

    // a steady fire fountain from the bottom center
    for (int n = 0; n < 6; n++) {
        float jx = (n - 3) * 2.0f;
        int warm[3] = { CLR_RED, CLR_ORANGE, CLR_YELLOW };
        spawn(SCREEN_W / 2.0f + jx, SCREEN_H - 10, jx * 0.15f, -1.8f - (n % 3) * 0.3f,
              50 + n * 5, 3.5f, warm[n % 3]);
    }

    for (int i = 0; i < MAX; i++) {
        if (ps[i].life <= 0) continue;
        ps[i].x += ps[i].vx;
        ps[i].y += ps[i].vy;
        ps[i].vy += 0.03f;     // gentle gravity so the fountain arcs
        ps[i].life -= 1;
    }
}

void draw(void) {
    cls(CLR_BLACK);

    // backdrop so the darkening modes (MUL/SUB) have light to act on: a bright mid band
    // (SUB carves into it toward black; MUL deepens+tints it) over a dim horizon strip.
    rectfill(0, 70, SCREEN_W, 46, CLR_LIGHT_PEACH);
    rectfill(0, 116, SCREEN_W, 24, CLR_MEDIUM_GREY);
    rectfill(0, SCREEN_H - 40, SCREEN_W, 40, CLR_DARKER_BLUE);
    rectfill(0, SCREEN_H - 40, SCREEN_W, 2, CLR_DARK_BLUE);

    if (mode != BLEND_NONE) blend(mode);
    for (int i = 0; i < MAX; i++) {
        if (ps[i].life <= 0) continue;
        float t = ps[i].life / ps[i].maxlife;      // 1→0 over its life
        float r = ps[i].size * (0.4f + 0.6f * t);
        circfill((int)ps[i].x, (int)ps[i].y, (int)r, ps[i].color);
    }
    blend_reset();

    // HUD
    const char *names[5] = { "OFF (overwrite)", "AVG (glass)", "ADD (glow)", "MUL (fog)", "SUB (carve)" };  // indexed by BLEND_* value
    print("blend:", 6, 6, CLR_LIGHT_GREY);
    print(names[mode], 44, 6, CLR_WHITE);
    print("1 ADD 2 AVG 3 MUL 4 SUB 0 OFF SPACE burst", 6, SCREEN_H - 10, CLR_MEDIUM_GREY);
}
