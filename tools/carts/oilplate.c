/* de:meta
{
  "title": "oil plate",
  "status": "active",
  "created": "2026-06-04",
  "kind": [
    "toy",
    "tech-demo"
  ],
  "teaches": [
    "voronoi"
  ],
  "description": "Liquid light show, Voronoi style. Three large coloured blobs (orange, blue, red) drift slowly; each pixel is owned by the blob with the highest field — flat colour regions with a bright iridescent seam exactly where two blobs meet. Small dark bubbles float inside the regions and get a bright ring for free from the same seam logic. A re-rolls."
}
de:meta */
#include "studio.h"
#include <math.h>

// oilplate — liquid light show, Voronoi style.
// Each pixel is owned by the blob with the highest r²/d² field value.
// Where top-2 are nearly equal → bright seam (the iridescent boundary line).
// Small dark bubbles are the same entity type; the seam logic gives them
// a bright ring for free.
// A = re-roll

#define NL    3          // large coloured blobs
#define NB    9          // small dark bubbles
#define NTOT (NL + NB)

static const int LCOLS[NL] = { CLR_DARK_ORANGE, CLR_TRUE_BLUE, CLR_DARK_RED };
#define BCOL  CLR_BLACK        // bubble / background interior
#define SEAM  CLR_LIGHT_PEACH  // iridescent boundary (warm cream, not harsh white)

static const int BAYER[4][4] = {
    { 0,  8,  2, 10},
    {12,  4, 14,  6},
    { 3, 11,  1,  9},
    {15,  7, 13,  5},
};

static float bx[NTOT], by[NTOT], bvx[NTOT], bvy[NTOT], br2[NTOT];
static bool  si = false;

static void reset(void) {
    // large coloured blobs — slow, big
    for (int i = 0; i < NL; i++) {
        bx[i]  = rnd_between(50, SCREEN_W - 50);
        by[i]  = rnd_between(40, SCREEN_H - 40);
        float spd = rnd_float_between(0.18f, 0.45f);
        bvx[i] = rnd_float() > 0.5f ?  spd : -spd;
        bvy[i] = rnd_float() > 0.5f ?  rnd_float_between(0.12f, 0.38f)
                                     : -rnd_float_between(0.12f, 0.38f);
        float r = rnd_float_between(72.0f, 96.0f);
        br2[i] = r * r;
    }
    // small dark bubbles — faster, small
    for (int i = NL; i < NTOT; i++) {
        bx[i]  = rnd_between(15, SCREEN_W - 15);
        by[i]  = rnd_between(15, SCREEN_H - 15);
        float spd = rnd_float_between(0.3f, 0.72f);
        bvx[i] = rnd_float() > 0.5f ?  spd : -spd;
        bvy[i] = rnd_float() > 0.5f ?  rnd_float_between(0.2f, 0.55f)
                                     : -rnd_float_between(0.2f, 0.55f);
        float r = rnd_float_between(14.0f, 28.0f);
        br2[i] = r * r;
    }
}

void draw(void) {
    if (!si) { reset(); si = true; }
    if (btn(0, BTN_A)) reset();

    // move all blobs
    for (int i = 0; i < NTOT; i++) {
        bx[i] += bvx[i]; by[i] += bvy[i];
        if (bx[i] < 5 || bx[i] > SCREEN_W - 5) bvx[i] = -bvx[i];
        if (by[i] < 5 || by[i] > SCREEN_H - 5) bvy[i] = -bvy[i];
        if (bx[i] < 5)          bx[i] = 5;
        if (bx[i] > SCREEN_W-5) bx[i] = SCREEN_W - 5;
        if (by[i] < 5)          by[i] = 5;
        if (by[i] > SCREEN_H-5) by[i] = SCREEN_H - 5;
    }

    for (int py = 0; py < SCREEN_H; py++) {
        for (int px = 0; px < SCREEN_W; px++) {

            // find top-2 field contributions
            float c1 = 0.0f, c2 = 0.0f;
            int   best = 0;
            for (int i = 0; i < NTOT; i++) {
                float dx = px - bx[i], dy = py - by[i];
                float c  = br2[i] / (dx*dx + dy*dy + 1.0f);
                if (c > c1) { c2 = c1; c1 = c; best = i; }
                else if (c > c2) c2 = c;
            }

            // dark background far from all blobs
            if (c1 < 0.35f) { pset(px, py, BCOL); continue; }

            int col = (best < NL) ? LCOLS[best] : BCOL;

            // iridescent seam: bright where two blobs nearly tie
            // ratio → 1 at the exact boundary, 0 deep inside a region
            float ratio = c2 / (c1 + 0.001f);
            float sf    = (ratio - 0.80f) * 5.0f;  // 0 at ratio=0.80, 1 at ratio=1.0
            if (sf > 0.0f) {
                if (sf > 1.0f) sf = 1.0f;
                float thr = (BAYER[py & 3][px & 3] + 0.5f) / 16.0f;
                if (sf > thr) col = SEAM;
            }

            pset(px, py, col);
        }
    }
}
