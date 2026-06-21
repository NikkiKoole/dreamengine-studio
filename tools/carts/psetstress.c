#include "studio.h"

// pset STRESS PROBE — not a game. Fills the whole screen with pset_rgb twice per
// frame (~128k pset calls/frame) to expose the per-pixel draw-call cost on a given
// backend. A/B the batched-pset path: native via env DE_BATCH_PSET=on; web via
// -DDE_BATCH_PSET_DEFAULT=1 (browsers have no env). See docs/design/software-canvas.md
// → "Cheaper alternatives". A moving color keeps the optimizer honest.
#ifndef STRESS_PASSES
#define STRESS_PASSES 2          // full-screen pset fills per frame (~64k psets each)
#endif
static int t = 0;

void update(void) { t++; }

void draw(void) {
    cls(0);
    for (int pass = 0; pass < STRESS_PASSES; pass++)
        for (int y = 0; y < SCREEN_H; y++)
            for (int x = 0; x < SCREEN_W; x++) {
                int r = (x + t) & 0xFF, g = (y + t) & 0xFF, b = (x ^ y) & 0xFF;
                pset_rgb(x, y, (r << 16) | (g << 8) | b);
            }
}
