// Chapter-13 payoff (a VIDEO, with sound): a whole little shooter, and every part of it
// is a chapter you already read. Arrays of bullets + enemies (9), does-it-touch (8),
// shake + particles on a kill (10), a note() on every shot (7), a score (12). It plays
// itself: the ship slides under the nearest enemy and fires.
#include "studio.h"

#define NB 16
#define NE 10
#define NP 32
#define SHIPY (SCREEN_H - 22)
static float bx[NB], by[NB];            static int balive[NB];
static float ex[NE], ey[NE];            static int ealive[NE];
static float px[NP], py[NP], pvx[NP], pvy[NP]; static int plife[NP];
static float shipx;
static int   score, shootcd, spawncd, ready;

void init(void) {
    shipx = SCREEN_W / 2; score = shootcd = spawncd = 0; ready = 1;
    for (int i = 0; i < NB; i++) balive[i] = 0;
    for (int i = 0; i < NE; i++) ealive[i] = 0;
    for (int i = 0; i < NP; i++) plife[i] = 0;
}

static void boom(float x, float y) {                 // chapter 10, in one function
    shake(3.0f);
    hit(38, INSTR_NOISE, 5, 130);                    // an explosion
    int made = 0;
    for (int i = 0; i < NP && made < 10; i++)
        if (plife[i] <= 0) {
            float a = made * 36.0f, sp = 1.0f + rnd(20) / 10.0f;
            px[i] = x; py[i] = y;
            pvx[i] = cos_deg(a) * sp; pvy[i] = sin_deg(a) * sp;
            plife[i] = 12 + rnd(8); made++;
        }
}

void update(void) {
    if (!ready) init();

    // --- attract AI: chase the nearest enemy, then shoot ---
    int near = -1; float nd = 1e9f;
    for (int i = 0; i < NE; i++)
        if (ealive[i]) { float d = ex[i] - shipx; if (d < 0) d = -d; if (d < nd) { nd = d; near = i; } }
    if (near >= 0) shipx += (ex[near] - shipx) * 0.12f;
    if (shipx < 12) shipx = 12; if (shipx > SCREEN_W - 12) shipx = SCREEN_W - 12;

    if (shootcd > 0) shootcd--;
    if (near >= 0 && shootcd == 0)
        for (int i = 0; i < NB; i++)
            if (!balive[i]) { bx[i] = shipx; by[i] = SHIPY - 8; balive[i] = 1; shootcd = 11;
                              note(84, INSTR_TRI, 2); break; }   // pew (chapter 7)

    // --- spawn enemies (chapter 9: fill a dead slot) ---
    if (spawncd > 0) spawncd--;
    if (spawncd == 0)
        for (int i = 0; i < NE; i++)
            if (!ealive[i]) { ex[i] = 20 + rnd(SCREEN_W - 40); ey[i] = -8; ealive[i] = 1;
                              spawncd = 26; break; }

    for (int i = 0; i < NB; i++) if (balive[i]) { by[i] -= 5; if (by[i] < 0) balive[i] = 0; }
    for (int i = 0; i < NE; i++) if (ealive[i]) { ey[i] += 0.9f; if (ey[i] > SCREEN_H) ealive[i] = 0; }

    // --- does it touch? (chapter 8) bullet vs enemy ---
    for (int b = 0; b < NB; b++) if (balive[b])
        for (int e = 0; e < NE; e++) if (ealive[e]) {
            float dx = bx[b] - ex[e], dy = by[b] - ey[e];
            if (dx * dx + dy * dy < 13 * 13) {
                balive[b] = 0; ealive[e] = 0; score++; boom(ex[e], ey[e]);
            }
        }

    for (int i = 0; i < NP; i++)
        if (plife[i] > 0) { pvy[i] += 0.12f; px[i] += pvx[i]; py[i] += pvy[i]; plife[i]--; }
}

void draw(void) {
    cls(CLR_DARK_BLUE);
    for (int i = 0; i < NP; i++)                       // explosions
        if (plife[i] > 0) circfill((int)px[i], (int)py[i], plife[i] > 6 ? 2 : 1,
                                   plife[i] > 8 ? CLR_YELLOW : CLR_ORANGE);
    for (int i = 0; i < NB; i++) if (balive[i]) rectfill((int)bx[i] - 1, (int)by[i], 2, 6, CLR_YELLOW);
    for (int i = 0; i < NE; i++) if (ealive[i]) {      // little invaders
        circfill((int)ex[i], (int)ey[i], 8, CLR_RED);
        circfill((int)ex[i] - 3, (int)ey[i] - 1, 2, CLR_WHITE);
        circfill((int)ex[i] + 3, (int)ey[i] - 1, 2, CLR_WHITE);
    }
    trifill((int)shipx, SHIPY - 10, (int)shipx - 9, SHIPY + 6, (int)shipx + 9, SHIPY + 6, CLR_BLUE);
    rectfill((int)shipx - 2, SHIPY - 4, 4, 8, CLR_LIGHT_GREY);
    print(str("score  %d", score), 8, 8, CLR_WHITE);
}
