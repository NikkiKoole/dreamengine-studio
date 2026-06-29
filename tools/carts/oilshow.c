/* de:meta
{
  "title": "oil show",
  "status": "active",
  "kind": [
    "toy",
    "tech-demo"
  ],
  "teaches": [
    "dithering-gradient"
  ],
  "lineage": "No direct cart lineage; the oily iridescent look is novel — a metaball scalar field feeds Bayer ordered-dithering between adjacent palette stops so colour transitions emerge from the field gradient, not special-cased edge logic.",
  "description": "Liquid light show — five metaballs drift across the screen; the scalar field at each pixel drives Bayer ordered-dithering between adjacent palette colours, giving the oily iridescent edges where blobs merge. The whole colour scheme slowly drifts through the rainbow. A re-rolls the blobs, B pauses the colour drift."
}
de:meta */
#include "studio.h"
#include <math.h>

// oilshow — liquid light show. Metaball scalar field drives Bayer ordered-dither
// blending between adjacent palette colours — the "oily" mixing at blob edges
// comes from the field gradient, not a special case.
// A = re-roll blobs   B = pause colour drift

#define NB  5   // blobs
#define NC  9   // colour cycle entries (index 0 == index NC-1 for seamless wrap)

static const int COLS[NC] = {
    CLR_DARK_PURPLE, CLR_DARK_RED,   CLR_DARK_ORANGE,
    CLR_ORANGE,      CLR_YELLOW,     CLR_LIME_GREEN,
    CLR_BLUE,        CLR_INDIGO,     CLR_DARK_PURPLE,
};

static const int BAYER[4][4] = {
    { 0,  8,  2, 10},
    {12,  4, 14,  6},
    { 3, 11,  1,  9},
    {15,  7, 13,  5},
};

static float bx[NB], by[NB], bvx[NB], bvy[NB];
static float phase  = 0.0f;
static bool  si     = false;
static bool  drift  = true;
static bool  prev_b = false;

static void reset(void) {
    for (int i = 0; i < NB; i++) {
        bx[i]  = rnd_between(30, SCREEN_W - 30);
        by[i]  = rnd_between(30, SCREEN_H - 30);
        // guaranteed non-zero speed, random sign per axis
        bvx[i] = rnd_float() > 0.5f ?  rnd_float_between(0.3f, 0.85f)
                                     : -rnd_float_between(0.3f, 0.85f);
        bvy[i] = rnd_float() > 0.5f ?  rnd_float_between(0.3f, 0.85f)
                                     : -rnd_float_between(0.3f, 0.85f);
    }
}

void draw(void) {
    if (!si) { reset(); si = true; }

    // input
    if (btn(0, BTN_A)) reset();
    bool cur_b = btn(0, BTN_B);
    if (cur_b && !prev_b) drift = !drift;
    prev_b = cur_b;

    // advance phase (slow colour drift)
    if (drift) {
        phase += 0.005f;
        if (phase >= 1.0f) phase -= 1.0f;
    }

    // move blobs
    for (int i = 0; i < NB; i++) {
        bx[i] += bvx[i];
        by[i] += bvy[i];
        if (bx[i] < 5 || bx[i] > SCREEN_W - 5) bvx[i] = -bvx[i];
        if (by[i] < 5 || by[i] > SCREEN_H - 5) bvy[i] = -bvy[i];
        if (bx[i] < 5)          bx[i] = 5;
        if (bx[i] > SCREEN_W-5) bx[i] = SCREEN_W - 5;
        if (by[i] < 5)          by[i] = 5;
        if (by[i] > SCREEN_H-5) by[i] = SCREEN_H - 5;
    }

    float ph = phase * (NC - 1);   // 0 .. NC-2

    // render — one pset per pixel, Bayer dithering between adjacent colour stops
    for (int py = 0; py < SCREEN_H; py++) {
        for (int px = 0; px < SCREEN_W; px++) {

            // metaball field: sum of r²/d² contributions (r=60 → r²=3600)
            float F = 0.0f;
            for (int i = 0; i < NB; i++) {
                float dx = px - bx[i], dy = py - by[i];
                F += 3600.0f / (dx*dx + dy*dy + 1.0f);
            }

            // map field → colour position in the cycle.
            // sqrt compresses the range so blob centres don't wrap many times;
            // ph shifts the whole palette slowly over time.
            float t = fmodf(sqrtf(F) * 0.55f + ph, (float)(NC - 1));

            int   lo   = (int)t;
            int   hi   = (lo + 1) % (NC - 1);
            float frac = t - (float)lo;

            // Bayer ordered dither picks lo or hi colour per pixel
            float thr = (BAYER[py & 3][px & 3] + 0.5f) / 16.0f;
            pset(px, py, COLS[frac > thr ? hi : lo]);
        }
    }
}
