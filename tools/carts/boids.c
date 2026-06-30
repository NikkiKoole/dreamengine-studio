/* de:meta
{
  "title": "boids",
  "status": "active",
  "created": "2026-05-30",
  "kind": [
    "tech-demo",
    "probe"
  ],
  "teaches": [
    "flocking"
  ],
  "description": "A flock with no leader, from three little rules. Each boid watches only nearby neighbours and obeys: separation (steer away from anyone too close), alignment (match the flock heading), cohesion (drift toward the flock centre). Nobody is in charge yet the swarm moves like one organism — emergence. Steer the predator with the arrows and watch the flock part and regroup around it.",
  "todo": [
    "Touch: add an onscreen joystick for steering."
  ]
}
de:meta */
#include "studio.h"
#include <math.h>

// BOIDS — a flock with no leader, from three little rules.
//
// Every boid looks only at its nearby neighbours and obeys three urges:
//   SEPARATION  steer away from anyone too close (don't collide)
//   ALIGNMENT   turn to match the average heading of the flock
//   COHESION    drift toward the average position of the flock
// Nobody is in charge, yet the swarm moves like one organism. That's emergence:
// complex group behaviour out of simple local rules. Steer the predator and
// watch the flock part and regroup around it.
//
//   arrows: move the predator (the flock flees it)

#define N      70
#define VIEW   26.0f
#define MAXSPD 2.2f

static float bx[N], by[N], vx[N], vy[N];
static float predx = 160, predy = 100;

static void make_flock(void) {
    for (int i = 0; i < N; i++) {
        bx[i] = rnd(SCREEN_W); by[i] = rnd(SCREEN_H);
        float a = rnd(360);
        vx[i] = cos_deg(a) * MAXSPD; vy[i] = sin_deg(a) * MAXSPD;
    }
}

static void step(void) {
    for (int i = 0; i < N; i++) {
        float sepx = 0, sepy = 0, alx = 0, aly = 0, cox = 0, coy = 0;
        int n = 0;
        for (int j = 0; j < N; j++) {
            if (j == i) continue;
            float dx = bx[j] - bx[i], dy = by[j] - by[i], d2 = dx * dx + dy * dy;
            if (d2 < VIEW * VIEW) {
                n++; alx += vx[j]; aly += vy[j]; cox += bx[j]; coy += by[j];
                if (d2 < 100) { sepx -= dx; sepy -= dy; }
            }
        }
        if (n > 0) {
            alx /= n; aly /= n; cox /= n; coy /= n;
            vx[i] += (alx - vx[i]) * 0.05f + (cox - bx[i]) * 0.0009f + sepx * 0.02f;
            vy[i] += (aly - vy[i]) * 0.05f + (coy - by[i]) * 0.0009f + sepy * 0.02f;
        }
        float pdx = bx[i] - predx, pdy = by[i] - predy, pd2 = pdx * pdx + pdy * pdy;
        if (pd2 < 3600) { vx[i] += pdx * 0.012f; vy[i] += pdy * 0.012f; }   // flee
        float sp = sqrtf(vx[i] * vx[i] + vy[i] * vy[i]);
        if (sp > MAXSPD) { vx[i] = vx[i] / sp * MAXSPD; vy[i] = vy[i] / sp * MAXSPD; }
        else if (sp > 0 && sp < 0.8f) { vx[i] = vx[i] / sp * 0.8f; vy[i] = vy[i] / sp * 0.8f; }
        bx[i] += vx[i]; by[i] += vy[i];
        if (bx[i] < 0) bx[i] += SCREEN_W; if (bx[i] >= SCREEN_W) bx[i] -= SCREEN_W;
        if (by[i] < 0) by[i] += SCREEN_H; if (by[i] >= SCREEN_H) by[i] -= SCREEN_H;
    }
}

void init(void) { make_flock(); for (int i = 0; i < 120; i++) step(); }

void update(void) {
    if (btn(0, BTN_LEFT))  predx -= 3;
    if (btn(0, BTN_RIGHT)) predx += 3;
    if (btn(0, BTN_UP))    predy -= 3;
    if (btn(0, BTN_DOWN))  predy += 3;
    predx = clamp(predx, 0, SCREEN_W); predy = clamp(predy, 0, SCREEN_H);
    step();
}

static void draw_boid(float x, float y, float dvx, float dvy, int col) {
    float sp = sqrtf(dvx * dvx + dvy * dvy); if (sp < 0.01f) sp = 0.01f;
    float nx = dvx / sp, ny = dvy / sp;
    trifill((int)(x + nx * 5), (int)(y + ny * 5),
            (int)(x - nx * 4 - ny * 3), (int)(y - ny * 4 + nx * 3),
            (int)(x - nx * 4 + ny * 3), (int)(y - ny * 4 - nx * 3), col);
}

void draw(void) {
    cls(CLR_DARKER_BLUE);
    for (int i = 0; i < N; i++) draw_boid(bx[i], by[i], vx[i], vy[i], CLR_LIME_GREEN);
    circfill((int)predx, (int)predy, 5, CLR_RED);
    circ((int)predx, (int)predy, 9, CLR_DARK_RED);
    print("BOIDS", 4, 4, CLR_LIGHT_GREY);
    print("arrows: move predator, flock flees", 4, SCREEN_H - 9, CLR_DARK_GREY);
}
