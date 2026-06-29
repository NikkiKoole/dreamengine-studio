/* de:meta
{
  "title": "rotstress — rotated-sprite stress test",
  "status": "active",
  "kind": [
    "probe"
  ],
  "teaches": [
    "software-rasterizer"
  ],
  "lineage": "The profiling target for the software-canvas rotated-blit optimization (de_cpu_img_rot) — a crankable worst case used to find the GetImageColor/sw_pset hotspots and measure each win.",
  "description": "A deliberate worst case for the software canvas rotated-sampling path: a storm of spinning sprites plus big rotated scaled blits and a textured quad, all opaque, so the per-pixel inverse-map + texel gather dominate. Built to be profiled — crank the sprite count with UP/DOWN to push past the frame budget and see where the time goes."
}
de:meta */
// rotstress — a deliberate WORST CASE for the software-canvas rotated-sampling path, built to be
// PROFILED. A storm of spinning sprites (spr_rot) + big rotated scaled blits (sspr_ex) + a spinning
// textured quad (tritex) — all opaque — so GetImageColor + the inverse-map inner loop (de_cpu_img_rot /
// sw_tritex) dominate the profile. Use it to find/verify the sampling hotspot and measure a fix.
//
// The path being optimized only runs under the software canvas, so profile THAT:
//   # cost (workMsAvg in build/perf.json), GPU vs canvas:
//   node tools/play.js rotstress run --headless --frames 120                 # GPU baseline
//   DE_SOFTWARE_CANVAS=on node tools/play.js rotstress run --headless --frames 120
//   # per-function hotspot: run the binary and `sample` it, or the editor ⏱ button
//   #   (set DE_SOFTWARE_CANVAS=on in the shell before `make` to profile the SW path).
//
// UP/DOWN = sprite count (dial the load until it's frame-budget-bound). Deterministic (frame()-driven),
// so two runs match — also A/B-able with canvas-diff.
#include "studio.h"
#include <math.h>
#include <stdio.h>

#include <stdlib.h>
static int count = 600;          // interactive default; UP/DOWN to dial
static int inited = 0;

void update(void) {
    if (btnp(0, BTN_UP))                 count += 64;
    if (btnp(0, BTN_DOWN) && count > 64) count -= 64;
}

void draw(void) {
    if (!inited) { const char *e = getenv("RS_COUNT"); if (e) count = atoi(e); inited = 1; }   // headless profiling knob
    int f = (int)frame();
    cls(CLR_BLACK);

    // sprite storm — scattered (overlapping = overdraw) spinning 16×16 spr_rot
    for (int i = 0; i < count; i++) {
        int gx = (i * 37) % (SCREEN_W - 16);
        int gy = (i * 53) % (SCREEN_H - 24);
        spr_rot(0, gx, gy, f * 3.0f + i * 11.0f);
    }

    // a row of BIG rotated scaled blits — most sampled pixels per call (purest sampling load)
    for (int i = 0; i < 6; i++)
        sspr_ex(0, 0, 16, 16, 10 + i * 50, 150, 48, 48, f * 2.0f + i * 30, 24, 24);

    // a spinning textured quad (the tritex sampling path)
    { int cx = 290, cy = 170; float r = 26, t = f * 0.05f;
      int ax=(int)(cx+cosf(t)*r),       ay=(int)(cy+sinf(t)*r);
      int bx=(int)(cx+cosf(t+1.57f)*r), by=(int)(cy+sinf(t+1.57f)*r);
      int dx=(int)(cx+cosf(t+3.14f)*r), dy=(int)(cy+sinf(t+3.14f)*r);
      int ex=(int)(cx+cosf(t+4.71f)*r), ey=(int)(cy+sinf(t+4.71f)*r);
      tritex(ax,ay,0,0,  bx,by,16,0,  dx,dy,16,16);
      tritex(ax,ay,0,0,  dx,dy,16,16, ex,ey,0,16); }

    print("rotstress", 4, 4, CLR_WHITE);
    char buf[32]; snprintf(buf, sizeof buf, "sprites:%d  up/down", count);
    print(buf, 4, 188, CLR_LIGHT_GREY);
#ifdef DE_TRACE
    watch("count", "%d", count);
#endif
}
