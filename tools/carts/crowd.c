/* de:meta
{
  "title": "the crowd",
  "status": "active",
  "created": "2026-05-30",
  "kind": [
    "tech-demo"
  ],
  "teaches": [],
  "description": "A plaza full of little 16×32 people walking, colliding and bouncing off each other and the screen edges — a stress test of the sprite system. The headline trick is pal()-recolored \"magic\" clothes: everyone shares just three sprite bodies, but each person's shirt (drawn in palette index 28) and pants (index 29) are swapped to random colors with pal() before drawing, so one tiny sprite set yields a whole varied crowd. Legs animate a 4-frame front-view march, agents are depth-sorted by their feet, and the ground is a real tile map() — grass, a cobbled crossroads, and scattered flowers. Z scatters everyone, X adds another person."
}
de:meta */
#include "studio.h"

// crowd — a plaza full of little 16×32 people walking, colliding and bouncing.
// A stress test of the sprite system + a demo of pal()-recolored "magic" clothes:
// every person shares the same 3 sprite bodies, but their SHIRT (palette index 28)
// and PANTS (index 29) are swapped to random colors per-agent with pal() before
// drawing. The ground is a real tile map drawn with map().
//
// Z = scatter everyone   X = add a person

#define MAGIC_SHIRT  28      // placeholder index the sprites paint shirts in
#define MAGIC_PANTS  29      // ...and pants

#define MAX_PEOPLE   28
#define BODY_R        5      // collision radius around the feet

typedef struct {
    float x, y;              // top-left of the 16×32 sprite
    float vx, vy;            // px/frame at 60fps
    int   body;              // top slot 0..2
    int   shirt, pants;      // palette indices to recolor into
    float phase;             // walk-cycle offset
} Person;

static Person people[MAX_PEOPLE];
static int    np = 0;
static int    order[MAX_PEOPLE];   // draw order, sorted back-to-front

// nice clothing palettes (avoid index 0 / the magic placeholders themselves)
static const int SHIRTS[] = { 8, 9, 10, 11, 12, 14, 7, 2, 26, 23, 30, 28 };
static const int PANTS[]  = { 1, 5, 4, 13, 16, 20, 24, 18, 12, 3 };

static void spawn(float x, float y) {
    if (np >= MAX_PEOPLE) return;
    Person *p = &people[np++];
    p->x = x; p->y = y;
    float dir = rnd(360);
    float spd = rnd_float_between(0.35f, 0.8f);
    p->vx = dx(spd, dir);
    p->vy = dy(spd, dir);
    p->body  = rnd(3);
    p->shirt = SHIRTS[rnd((int)(sizeof SHIRTS / sizeof *SHIRTS))];
    p->pants = PANTS[rnd((int)(sizeof PANTS / sizeof *PANTS))];
    p->phase = rnd_float();
}

void init() {
    colorkey(0);                 // index 0 = transparent for sprites
    for (int i = 0; i < 22; i++)
        spawn(rnd(SCREEN_W - 16), rnd(SCREEN_H - 32));

    // a soft ambient pad
    instrument(5, INSTR_TRI, 250, 120, 4, 500);
    instrument_filter(5, FILTER_LOW, 900, 4);
    bpm(80);
}

void update() {
    float k = dt() * 60.0f;      // frame-rate independent step

    if (btnp(0, BTN_A))          // scatter: re-fling everyone
        for (int i = 0; i < np; i++) {
            float dir = rnd(360), spd = rnd_float_between(0.5f, 1.2f);
            people[i].vx = dx(spd, dir);
            people[i].vy = dy(spd, dir);
        }
    if (btnp(0, BTN_B)) spawn(rnd(SCREEN_W - 16), rnd(SCREEN_H - 32));

    // move + bounce off the arena edges
    for (int i = 0; i < np; i++) {
        Person *p = &people[i];
        p->x += p->vx * k;
        p->y += p->vy * k;
        if (p->x < 0)              { p->x = 0;              p->vx = -p->vx; }
        if (p->x > SCREEN_W - 16)  { p->x = SCREEN_W - 16;  p->vx = -p->vx; }
        if (p->y < 0)              { p->y = 0;              p->vy = -p->vy; }
        if (p->y > SCREEN_H - 32)  { p->y = SCREEN_H - 32;  p->vy = -p->vy; }
    }

    // pairwise collisions at the feet — separate + elastic equal-mass bounce
    float minD = BODY_R * 2;
    for (int i = 0; i < np; i++)
        for (int j = i + 1; j < np; j++) {
            Person *a = &people[i], *b = &people[j];
            float ax = a->x + 8, ay = a->y + 26;
            float bx = b->x + 8, by = b->y + 26;
            float ddx = bx - ax, ddy = by - ay;
            float d = fsqrt(ddx * ddx + ddy * ddy);
            if (d >= minD || d <= 0.001f) continue;
            float nx = ddx / d, ny = ddy / d;
            float overlap = (minD - d) * 0.5f;
            a->x -= nx * overlap; a->y -= ny * overlap;
            b->x += nx * overlap; b->y += ny * overlap;
            float rv = (b->vx - a->vx) * nx + (b->vy - a->vy) * ny;
            if (rv < 0) {                       // only if closing
                a->vx += rv * nx; a->vy += rv * ny;
                b->vx -= rv * nx; b->vy -= rv * ny;
            }
        }

    // ambient pad + soft footsteps
    if (every(8)) chord(48, CHORD_MAJ7, 5, 2);
    if (frame() % 16 == 0) hit(36, INSTR_NOISE, 1, 25);
}

// insertion-sort draw order by foot y so nearer people overlap farther ones
static void sort_order() {
    for (int i = 0; i < np; i++) order[i] = i;
    for (int i = 1; i < np; i++) {
        int v = order[i], j = i - 1;
        while (j >= 0 && people[order[j]].y > people[v].y) { order[j + 1] = order[j]; j--; }
        order[j + 1] = v;
    }
}

void draw() {
    cls(3);                              // dark green, in case the map has gaps
    map(0, 0, 0, 0, MAP_W, MAP_H);       // the tiled plaza floor

    sort_order();
    for (int n = 0; n < np; n++) {
        Person *p = &people[order[n]];
        int ix = (int)(p->x + 0.5f), iy = (int)(p->y + 0.5f);

        // soft shadow on the ground
        ovalfill(ix + 8, iy + 31, 6, 2, 1);

        int legf = anim(4, 7.0f, p->phase);     // 0..3 march cycle
        pal(MAGIC_SHIRT, p->shirt);             // <-- the magic: recolor this agent
        pal(MAGIC_PANTS, p->pants);
        spr(p->body, ix, iy);                   // top  (head + upper torso)
        spr(8 + legf, ix, iy + 16);             // legs (shared frame)
    }
    pal_reset();

    print(str("the crowd  -  %d people", np), 6, 5, 7);
    print("z scatter   x add", 6, SCREEN_H - 11, 6);
}
