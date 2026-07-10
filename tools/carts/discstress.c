/* de:meta
{
  "slug": "discstress",
  "title": "discstress",
  "status": "active",
  "created": "2026-06-17",
  "kind": [
    "tech-demo"
  ],
  "teaches": [],
  "description": "A deterministic torture test for the SOFTWARE disc/ellipse fills (circfill -> disc_inside -> plot_pat, ovalfill -> ellipse_inside -> plot_pat) — the circfill/ovalfill sibling of polystress. Scene 0 SOLID (key 1, the span-fill optimization target): three big pulsing circles (large fill AREA), a 60-cell grid of small circles (fill COUNT + per-call bbox setup), wide/tall/round ovals (ellipse_inside across aspect ratios), a big circle running OFF-SCREEN (there's no scan clamp on the disc path today), and r=0/1 degenerate radii. Scene 1 DITHER (key 2): big circles + ovals through every FILL_* pattern (the plot_pat fallback). Scene 2 ZOOM (key 3): planet-style big fills under a zooming camera_ex (rotation 0) — the byte-identity test for the span fast-path under zoom (orbit's case). Pure function of the frame counter — byte-reproducible — so it pins the disc rasterizer for a clean profile AND validates the span-fill optimization pixel-identical. Not a game; a profiling + regression rig."
}
de:meta */
// discstress — a deterministic torture test for the SOFTWARE disc/ellipse fills
// (circfill → disc_inside → plot_pat, ovalfill → ellipse_inside → plot_pat). The
// circfill/ovalfill sibling of polystress: pins the per-pixel disc path so a profile
// is unambiguous, and is byte-reproducible frame-by-frame so the span-fill optimization
// can be VALIDATED pixel-identical (--dump + shasum) and A/B'd.
//
// Everything is a pure function of the frame counter F — no time, no random. Default
// with no input stays on scene 0, so headless dumps are deterministic.
//   keys:  1 = SOLID scene (default/opening)   2 = DITHER scene
//   stress:    node tools/play.js discstress run
//   profile:   editor ▶ run, then ⏱ profile
//   validate:  node tools/play.js discstress script /dev/null --headless --frames 90 \
//                   --dump /tmp/d_before --dump-every 15 ; shasum /tmp/d_before/*.png
//
// SCENE 0 — SOLID: the optimization target (solid → one DrawRectangle per row). Three
//   big pulsing circles (large fill AREA × per-pixel disc_inside), a dense grid of small
//   circles (fill COUNT + per-call bbox setup), wide/tall/round ovals (ellipse_inside),
//   a big circle that runs OFF-SCREEN (no clamp today → scans its whole bbox), and r=0/1
//   degenerate radii.
// SCENE 1 — DITHER: big circles + ovals filled through every FILL_* pattern (the plot_pat
//   fallback path the solid fast-path won't take). Captures every dither run-length.

#include "studio.h"
#include <math.h>
#include <stdio.h>

#define TAU 6.28318530718f

static int F = 0;
static int scene = 0;                       // 0=SOLID (opening), 1=DITHER, 2=ZOOM
void update(void) {
    F++;
    if (keyp('1')) scene = 0;
    if (keyp('2')) scene = 1;
    if (keyp('3')) scene = 2;
}

// SCENE 2 — big circles/ovals under a ZOOMING camera_ex (rotation 0). The span fast-path
// is now gated on rotation==0 (zoom allowed), so this is the byte-identity test for that:
// the planet-style big fills (orbit's case) must come out pixel-identical to the per-pixel
// path (DE_DISC_FILL=legacy) under zoom. The big circle exceeds the screen at high zoom, so
// it also exercises poly_clamp_scan's clamp-under-zoom (fast clamps, legacy doesn't → same canvas).
static int draw_zoom(float ph) {
    int cx = SCREEN_W / 2, cy = SCREEN_H / 2;
    float z = 1.7f + 0.7f * sinf(ph);                  // zoom varies, rotation 0
    camera_ex(cx, cy, z, 0);
    int fills = 0;
    circfill(cx, cy, 58, CLR_DARK_GREEN);   fills++;   // a big "planet" (overspills the screen at high zoom)
    circfill(cx - 16, cy - 12, 26, CLR_MEDIUM_GREEN); fills++;
    circfill(cx + 18, cy + 10, 18, CLR_GREEN); fills++;
    ovalfill(cx, cy - 34, 28, 15, CLR_BLUE); fills++;
    circ(cx, cy, 58, CLR_LIME_GREEN);                  // outline (per-pixel disc_inside, unaffected)
    camera(0, 0);                                      // reset for HUD
    return fills;
}

static const struct { int pat; const char *name; } DITHERS[6] = {
    { FILL_CHECKER, "CHECKER" }, { FILL_DOTS,   "DOTS"   }, { FILL_HLINES, "HLINES" },
    { FILL_VLINES,  "VLINES"  }, { FILL_DIAG,   "DIAG"   }, { FILL_GRID,   "GRID"   },
};

// SCENE 0 — solid discs + ovals (the DrawRectangle-span target)
static int draw_solid(float ph) {
    int fills = 0;
    for (int s = 0; s < 3; s++) {           // big pulsing circles: large fill area
        float cx = SCREEN_W * (0.25f + 0.25f*s);
        float cy = SCREEN_H * 0.42f;
        int r = (int)(32 + 12 * sinf(ph + s));
        circfill((int)cx, (int)cy, r, 8 + (s + F/20) % 8); fills++;
    }
    int GX = 12, GY = 5;                     // dense grid of small circles: COUNT stress
    for (int gy = 0; gy < GY; gy++)
        for (int gx = 0; gx < GX; gx++) {
            int cx = (int)((gx + 0.5f) * SCREEN_W / GX);
            int cy = (int)(SCREEN_H * 0.70f + (gy) * 6);
            int r  = 4 + (gx + gy + F/15) % 4;
            circfill(cx, cy, r, 16 + (gx + gy) % 16); fills++;
        }
    // ovals: wide, tall, round — ellipse_inside across aspect ratios
    ovalfill(SCREEN_W/4,   30, (int)(34 + 10*sinf(ph)),       8,                       CLR_RED);    fills++;
    ovalfill(SCREEN_W/2,   30,  9,                            (int)(20 + 8*cosf(ph)),  CLR_GREEN);  fills++;
    ovalfill(3*SCREEN_W/4, 30, (int)(22 + 6*sinf(ph*1.3f)), (int)(22 + 6*cosf(ph*1.3f)), CLR_BLUE); fills++;
    // a big circle running OFF-SCREEN (exercises the missing clamp)
    circfill(8, SCREEN_H/2, 70 + (int)(10*sinf(ph*0.5f)), CLR_DARK_PURPLE); fills++;
    // degenerate radii (must not crash / draw wrong)
    circfill(SCREEN_W-6, SCREEN_H-6, 0, CLR_WHITE);
    circfill(SCREEN_W-6, SCREEN_H-12, 1, CLR_WHITE); fills += 2;
    return fills;
}

// SCENE 1 — dithered discs + ovals (the plot_pat fallback path)
static int draw_dither(float ph) {
    int fills = 0, GX = 3, GY = 2;
    for (int i = 0; i < 6; i++) {
        int gx = i % GX, gy = i / GX;
        int cx = (int)((gx + 0.5f) * SCREEN_W / GX);
        int cy = (int)((gy + 0.5f) * SCREEN_H / GY);
        int r  = (int)(40 + 6 * sinf(ph + i));
        fillp(DITHERS[i].pat, -1);
        if (i & 1) ovalfill(cx, cy, r, (int)(r*0.6f), 8 + i);
        else       circfill(cx, cy, r, 8 + i);
        fillp_reset();
        fills++;
        print(DITHERS[i].name, cx - 14, cy - 2, CLR_WHITE);
    }
    return fills;
}

void draw(void) {
    cls(CLR_BLACK);
    float ph = F * 0.04f;
    int fills = (scene == 0) ? draw_solid(ph) : (scene == 1) ? draw_dither(ph) : draw_zoom(ph);

    print(scene == 0 ? "discstress  SOLID (1)" : scene == 1 ? "discstress  DITHER (2)" : "discstress  ZOOM (3)", 4, 4, CLR_WHITE);
    char buf[40];
    snprintf(buf, sizeof buf, "F=%d fills=%d", F, fills);
    print(buf, 4, 12, CLR_LIGHT_GREY);
#ifdef DE_TRACE
    watch("fills", "%d", fills);
    watch("scene", "%d", scene);
#endif
}
