/* de:meta
{
  "title": "clampstress",
  "status": "active",
  "kind": [
    "tech-demo"
  ],
  "teaches": [],
  "description": "A deterministic torture test for the PER-CALL clamp cost shared by every software fill. Each circfill/ovalfill/polyfill/trifill calls poly_clamp_scan, which does 4 GetScreenToWorld2D (camera-matrix inverses) to find the on-screen scan box — but that box is constant for the whole frame unless the camera moves. This cart issues ~1500 TINY fills/frame (377 each of all four primitives, near-zero fill AREA), so the per-CALL clamp dominates — the rig to measure the ceiling of a per-frame clamp-box cache. Scene 0 STATIC (key 1, no camera: box identical every call). Scene 1 PAN (key 2: camera() pans each frame, so the cache must recompute once/frame and stay correct). Pure function of the frame counter — byte-reproducible. Baseline (per-call clamp, current): STATIC 1.90ms avg / 3.36ms max. Not a game; a profiling + regression rig."
}
de:meta */
// clampstress — torture test for the PER-CALL clamp cost shared by every software fill.
// Every circfill/ovalfill/polyfill/trifill calls poly_clamp_scan, which does 4
// GetScreenToWorld2D (camera-matrix inverses) to find the on-screen scan box — yet that
// box is constant for the whole frame unless the cart moves the camera. This cart issues
// ~1500 TINY fills/frame (minimal fill AREA, so the per-CALL clamp dominates), to measure
// the ceiling of a per-frame clamp-box cache. Deterministic (frame counter).
//   keys: 1 = STATIC (no camera — box identical every call/frame)
//         2 = PAN    (camera() moves each frame — cache must recompute once/frame & stay correct)
// A/B the cache when it lands: DE_CLAMP_CACHE=off vs on (or whatever flag it ships behind).

#include "studio.h"
#include <math.h>
#include <stdio.h>

static int F = 0, scene = 0;
void update(void) { F++; if (keyp('1')) scene = 0; if (keyp('2')) scene = 1; }

// one tiny fill per cell, cycling the four software-fill primitives so the cache is
// exercised through every poly_clamp_scan caller. Tiny radii ⇒ near-zero fill area ⇒
// the 4 matrix-inverts per call are the cost.
static int tiny_grid(float ph) {
    int fills = 0, GX = 48, GY = 31;
    for (int gy = 0; gy < GY; gy++)
        for (int gx = 0; gx < GX; gx++) {
            int x = (int)((gx + 0.5f) * SCREEN_W / GX);
            int y = (int)((gy + 0.5f) * SCREEN_H / GY);
            int j = (int)(ph + gx * 0.2f) & 1;       // tiny deterministic jitter
            int col = 8 + ((gx + gy + F / 12) % 24);
            switch ((gx + gy) & 3) {
                case 0: circfill(x, y, 2, col); break;
                case 1: ovalfill(x, y, 2, 3, col); break;
                case 2: {
                    int xy[8] = { x-2,y-2+j, x+2,y-2, x+2,y+2, x-2,y+2 };  // tiny quad
                    polyfill(xy, 4, col);
                } break;
                default: trifill(x-2, y+2, x+2+j, y+2, x, y-2, col); break;  // tiny tri
            }
            fills++;
        }
    return fills;
}

void draw(void) {
    cls(CLR_BLACK);
    float ph = F * 0.04f;
    if (scene == 1) camera((int)(20 * sinf(ph)), (int)(12 * cosf(ph)));  // pan → invalidates the cache each frame
    int fills = tiny_grid(ph);
    if (scene == 1) camera(0, 0);                                        // reset so the HUD draws unshifted

    print(scene == 0 ? "clampstress  STATIC (1)" : "clampstress  PAN (2)", 4, 4, CLR_WHITE);
    char buf[40];
    snprintf(buf, sizeof buf, "F=%d fills=%d (tiny, clamp-bound)", F, fills);
    print(buf, 4, 12, CLR_LIGHT_GREY);
#ifdef DE_TRACE
    watch("fills", "%d", fills);
    watch("scene", "%d", scene);
#endif
}
