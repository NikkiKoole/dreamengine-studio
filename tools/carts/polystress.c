/* de:meta
{
  "title": "polystress",
  "status": "active",
  "kind": [
    "tech-demo"
  ],
  "teaches": [],
  "description": "A deterministic torture test for the SOFTWARE polygon rasterizer (polyfill -> poly_fill_cov -> poly_inside -> plot_pat) — polyfill's sibling to trifill stress. 159 fills/frame across every path through the filler: three big concave 9-point stars (large fill AREA through the per-pixel even-odd test), a 70-cell grid of small spinning stars (polyfill COUNT / per-call setup), six sweeping road strips (convex tapering quads, exactly roadlab's fill_strip case), one checker-dithered concave star (plot_pat's fillp() patterned branch), and a giant mostly-off-screen quad (poly_clamp_scan). Everything is a pure function of the frame counter — no time, no random — so dumped frames are byte-reproducible run-to-run and build-to-build, which is the point: it pins the rasterizer for a clean profile (hit the ⏱ profile button) AND validates a rasterizer optimization pixel-identical (--dump + shasum before/after). Not a game; a profiling + regression rig."
}
de:meta */
// polystress — a deterministic torture test for the SOFTWARE polygon rasterizer
// (polyfill → poly_fill_cov → poly_inside → plot_pat). Built to (a) pin the CPU
// hard so a profile of the rasterizer is unambiguous, and (b) be byte-reproducible
// frame-by-frame so an optimization can be VALIDATED pixel-identical.
//
// Everything is a pure function of the frame counter F — no time, no random — so
// `play.js polystress --dump` produces the same PNGs on every run and on every build.
// Default-with-no-input stays on scene 0, so headless dumps are deterministic.
//   keys:  1 = DITHER scene (default/opening)   2 = MIXED scene
//   stress:    node tools/play.js polystress run
//   profile:   editor ▶ run, then ⏱ profile  (or play.js headless + perf.json)
//   validate:  node tools/play.js polystress script /dev/null --headless --frames 90 \
//                   --dump /tmp/poly_before --dump-every 15 ; shasum /tmp/poly_before/*.png
//   A/B:       prefix DE_POLY_FILL=legacy to use the old per-pixel fill (guides/engine-optimization.md)
//
// SCENE 0 — DITHER: six big concave stars, each filled with a different FILL_* pattern
//   (CHECKER/DOTS/HLINES/VLINES/DIAG/GRID), so one profile captures every dither
//   run-length — CHECKER alternates every pixel (worst case for span batching), DOTS/
//   HLINES have long runs (best case). This is the target for measuring draw_span_pat.
// SCENE 1 — MIXED: solid big stars (large-area even-odd) + a 70-cell star grid (fill
//   COUNT) + roadlab-style convex strips + one dithered star + a giant off-screen quad
//   (poly_clamp_scan). The all-round rasterizer stress.

#include "studio.h"
#include <math.h>
#include <stdio.h>

#define TAU 6.28318530718f

static int F = 0;
static int scene = 0;                       // 0=DITHER (opening), 1=MIXED
void update(void) {
    F++;
    if (keyp('1')) scene = 0;
    if (keyp('2')) scene = 1;
}

// concave n-point star into xy[], returns vertex count (2*points)
static int mkstar(float cx, float cy, float rOut, float rIn, int points, float rot, int *xy) {
    int n = points * 2;
    for (int i = 0; i < n; i++) {
        float a = rot + (float)i / n * TAU;
        float r = (i & 1) ? rIn : rOut;
        xy[i*2]   = (int)(cx + cosf(a) * r);
        xy[i*2+1] = (int)(cy + sinf(a) * r);
    }
    return n;
}

// a roadlab-style strip: a centreline swept into convex quad segments, filled one
// polyfill per segment (this is fill_strip, the convex-trapezoid case, distilled)
static void strip(float x0, float y0, float x1, float y1, float halfw, int segs, int col) {
    float dx = x1 - x0, dy = y1 - y0;
    float L = sqrtf(dx*dx + dy*dy); if (L < 0.001f) L = 1;
    float nx = -dy / L * halfw, ny = dx / L * halfw;
    for (int i = 0; i < segs; i++) {
        float t0 = (float)i / segs, t1 = (float)(i+1) / segs;
        float ax = x0 + dx*t0, ay = y0 + dy*t0;
        float bx = x0 + dx*t1, by = y0 + dy*t1;
        int xy[8] = { (int)(ax+nx),(int)(ay+ny), (int)(bx+nx),(int)(by+ny),
                      (int)(bx-nx),(int)(by-ny), (int)(ax-nx),(int)(ay-ny) };
        polyfill(xy, 4, col);
    }
}

static const struct { int pat; const char *name; } DITHERS[6] = {
    { FILL_CHECKER, "CHECKER" }, { FILL_DOTS,   "DOTS"   }, { FILL_HLINES, "HLINES" },
    { FILL_VLINES,  "VLINES"  }, { FILL_DIAG,   "DIAG"   }, { FILL_GRID,   "GRID"   },
};

// SCENE 0 — six big DITHERED stars, one pattern each (the draw_span_pat target)
static int draw_dither(float ph) {
    int xy[64], fills = 0, GX = 3, GY = 2;
    for (int i = 0; i < 6; i++) {
        int gx = i % GX, gy = i / GX;
        float cx = (gx + 0.5f) * SCREEN_W / GX;
        float cy = (gy + 0.5f) * SCREEN_H / GY;
        float r  = 46 + 6 * sinf(ph + i);
        int n = mkstar(cx, cy, r, r*0.45f, 8, ph*0.5f + i, xy);
        fillp(DITHERS[i].pat, -1);
        polyfill(xy, n, 8 + i);             // 0-bits = colour, 1-bits = transparent
        fillp_reset();
        fills++;
        print(DITHERS[i].name, (int)cx - 14, (int)cy - 2, CLR_WHITE);
    }
    return fills;
}

// SCENE 1 — the all-round mixed rasterizer stress
static int draw_mixed(float ph) {
    int xy[64], fills = 0;
    for (int s = 0; s < 3; s++) {           // big concave stars: large fill area
        float cx = SCREEN_W * (0.25f + 0.25f*s), cy = SCREEN_H * 0.5f;
        float r  = 46 + 8 * sinf(ph + s);
        int n = mkstar(cx, cy, r, r*0.45f, 9, ph * (s+1) * 0.5f, xy);
        polyfill(xy, n, 8 + (s + F/20) % 8); fills++;
    }
    int GX = 10, GY = 7;                     // dense grid: polyfill COUNT
    for (int gy = 0; gy < GY; gy++)
        for (int gx = 0; gx < GX; gx++) {
            float cx = (gx + 0.5f) * SCREEN_W / GX, cy = (gy + 0.5f) * SCREEN_H / GY;
            int n = mkstar(cx, cy, 11, 4.5f, 5, ph*2 + (gx+gy), xy);
            polyfill(xy, n, 16 + (gx + gy + F/10) % 16); fills++;
        }
    for (int k = 0; k < 6; k++) {            // sweeping convex road strips
        float a = ph * 0.7f + k * (TAU / 6);
        float cx = SCREEN_W * 0.5f, cy = SCREEN_H * 0.5f;
        strip(cx, cy, cx + cosf(a)*140, cy + sinf(a)*100, 6.f, 14, CLR_DARK_GREY); fills += 14;
    }
    fillp(FILL_CHECKER, -1);                 // one dithered concave poly
    int n = mkstar(SCREEN_W*0.5f, SCREEN_H*0.5f, 70, 26, 7, -ph, xy);
    polyfill(xy, n, CLR_WHITE); fills++;
    fillp_reset();
    int g[8] = { (int)(-200 + cosf(ph*0.3f)*40), -150, (int)(SCREEN_W+200), (int)(-80 + sinf(ph*0.3f)*40),
                 (int)(SCREEN_W+150), SCREEN_H+200, -180, SCREEN_H+120 };   // giant off-screen quad
    fillp(FILL_DOTS, -1);
    polyfill(g, 4, CLR_DARK_BLUE); fills++;
    fillp_reset();
    return fills;
}

void draw(void) {
    cls(CLR_BLACK);
    float ph = F * 0.04f;
    int fills = (scene == 0) ? draw_dither(ph) : draw_mixed(ph);

    print(scene == 0 ? "polystress  DITHER (1)" : "polystress  MIXED (2)", 4, 4, CLR_WHITE);
    char buf[40];
    snprintf(buf, sizeof buf, "F=%d fills=%d", F, fills);
    print(buf, 4, 12, CLR_LIGHT_GREY);
#ifdef DE_TRACE
    watch("fills", "%d", fills);
    watch("scene", "%d", scene);
#endif
}
