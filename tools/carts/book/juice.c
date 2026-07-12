// Chapter-10 illustration: the SAME bounce as chapter 2, but juiced. Every time the
// ball lands it squashes, flings dust, flashes a ring, and kicks the whole screen with
// the built-in shake(). Each effect is tied to one event: the impact.
#include "studio.h"

#define NP 18
#define GROUND 150
#define BX 160
static float by, vy;
static float px[NP], py[NP], pvx[NP], pvy[NP];
static int   plife[NP], squash, flash, ready;

static void impact(void) {
    squash = 6; flash = 5;                         // feedbacks, all tied to one event
    shake(4.0f);                                   // the built-in screen kick (self-decaying)
    for (int i = 0; i < NP; i++) {                 // a burst of dust
        px[i] = BX; py[i] = GROUND;
        float a = 200 + i * (140.0f / NP);         // fan out and upward
        float sp = 1.5f + rnd(20) / 10.0f;
        pvx[i] = cos_deg(a) * sp;
        pvy[i] = sin_deg(a) * sp - 1.2f;
        plife[i] = 14 + rnd(8);
    }
}

void init(void) { by = 30; vy = 0; ready = 1; squash = flash = 0;
                  for (int i = 0; i < NP; i++) plife[i] = 0; }

void update(void) {
    if (!ready) init();
    vy += 0.6f; by += vy;
    if (by > GROUND - 12) { by = GROUND - 12; vy = -9.5f; impact(); }
    if (squash > 0) squash--;
    if (flash > 0) flash--;
    for (int i = 0; i < NP; i++)
        if (plife[i] > 0) { pvy[i] += 0.35f; px[i] += pvx[i]; py[i] += pvy[i]; plife[i]--; }
}

void draw(void) {
    cls(CLR_DARK_BLUE);
    line(0, GROUND, SCREEN_W, GROUND, CLR_DARK_GREEN);
    for (int i = 0; i < NP; i++)
        if (plife[i] > 0)
            circfill((int)px[i], (int)py[i], plife[i] > 6 ? 2 : 1,
                     plife[i] > 8 ? CLR_YELLOW : CLR_ORANGE);
    if (squash > 0) ovalfill(BX, GROUND - 6, 16, 8, CLR_YELLOW);   // squash on landing
    else            circfill(BX, (int)by, 12, CLR_YELLOW);
    if (flash > 0)  circ(BX, GROUND - 6, 14 + (5 - flash) * 4, CLR_WHITE);  // expanding ring
    print("shake + flash + squash + dust", 8, 8, CLR_LIGHT_GREY);
}
