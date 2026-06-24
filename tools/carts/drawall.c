// drawall — the EVERYTHING cart: exercises every draw command in studio.h, once per frame, with
// per-frame rotation + jitter (NOT a rotating camera, so it stays on the software canvas). This is
// the single cart to throw the whole draw layer at — canvas-diff / build-all / a visual look all
// hit it, so a regression in any primitive shows up in one place.
//
//   ┌───────────────────────────────────────────────────────────────────────────────────┐
//   │  WHEN YOU ADD A NEW DRAW COMMAND TO studio.h, ADD A CALL TO IT HERE.                 │
//   │  (the four-places rule in CLAUDE.md "Adding a new API function" points back here)    │
//   └───────────────────────────────────────────────────────────────────────────────────┘
//
// Deterministic by construction: all motion is driven by frame() only — no now()/timer() and no
// unseeded rnd() — so canvas-diff's two runs (GPU vs software canvas) render identical frames.
// Verify the whole draw layer in one shot:
//   node tools/canvas-diff.js drawall --max 80 --frames 8   # GPU vs DE_SOFTWARE_CANVAS, every primitive
//   node tools/canvas-diff.js drawall --raw     --frames 8  # vs the TRUE GPU rasterizers
// Why --max 80 (not 0): it includes SCALED sspr + zoom_rect, whose CPU nearest sampling differs from
// GPU POINT-center by a handful of edge pixels (the documented scaled-blit caveat) — ~48px. Everything
// else is byte-exact; this cart is the "every primitive renders + nothing corrupts" gate, while the
// per-primitive 0px byte-exactness lives in cityview/masseffect/advancewars/etc.
#include "studio.h"
#include <math.h>

static int poly_xy[8];

void draw(void) {
    int   f   = (int)frame();
    float t   = f * 0.04f;
    float deg = f * 2.0f;                       // continuous spin for the rotated primitives
    int   jit = (int)(sinf(t) * 4.0f);         // small deterministic jitter (camera test)

    static int inited = 0;
    if (!inited) { colorkey(8); enable_pget(true); inited = 1; }   // key red out of slot 2; allow pget reads

    cls(CLR_BLACK);

    // ── pixels + lines ────────────────────────────────────────────────────────────────────────
    pset(6, 6, CLR_WHITE);
    pset_rgb(10, 6, 0xFF8800);
    line(20, 4, 60, 16, CLR_RED);
    bezier(64, 16, 84, 0, 104, 16, CLR_GREEN);
    thickline(110, 4, 150, 16, 3, CLR_BLUE);
    thicklineoutline(110, 4, 150, 16, 3, CLR_WHITE);

    // ── rects (incl rotated) + bar + fillp dither ───────────────────────────────────────────────
    rect(4, 24, 20, 16, CLR_YELLOW);
    rectfill(28, 24, 20, 16, CLR_ORANGE);
    rectfill_rgb(52, 24, 20, 16, 0x4488FF);
    rrect(76, 24, 20, 16, 5, CLR_PINK);
    rrectfill(100, 24, 20, 16, 5, CLR_GREEN);
    rectfill_rot(140, 32, 22, 14, deg, CLR_RED);                  // ROTATING
    bar(166, 26, 40, 8, sinf(t) * 0.5f + 0.5f, CLR_GREEN, CLR_DARK_GREY);
    fillp(0x5A5A, -1); rectfill(214, 24, 24, 16, CLR_WHITE); fillp_reset();

    // ── round things (arcs/rings rotate their sweep) ───────────────────────────────────────────
    circ(14, 60, 9, CLR_WHITE);
    circfill(40, 60, 9, CLR_RED);
    oval(66, 60, 12, 8, CLR_GREEN);
    ovalfill(94, 60, 12, 8, CLR_BLUE);
    arc(124, 60, 10, 0, 220, CLR_YELLOW);
    arcfill(150, 60, 10, deg, deg + 120, CLR_ORANGE);            // ROTATING sweep
    arcoutline(176, 60, 10, deg, deg + 120, CLR_WHITE);
    ring(204, 60, 5, 10, 0, 300, CLR_PINK);
    ringoutline(232, 60, 5, 10, 0, 300, CLR_INDIGO);

    // ── polygons (ngon/star rotate; poly via array) ────────────────────────────────────────────
    tri(8, 92, 28, 78, 30, 98, CLR_RED);
    trifill(36, 92, 56, 78, 58, 98, CLR_GREEN);
    ngon(82, 88, 11, 5, deg, CLR_YELLOW);                        // ROTATING
    ngonfill(112, 88, 11, 6, deg, CLR_BLUE);
    star(144, 88, 12, 5, 5, deg, CLR_WHITE);                     // ROTATING
    starfill(178, 88, 12, 5, 6, deg, CLR_ORANGE);
    poly_xy[0]=206; poly_xy[1]=80; poly_xy[2]=226; poly_xy[3]=84;
    poly_xy[4]=220; poly_xy[5]=98; poly_xy[6]=208; poly_xy[7]=94;
    polyfill(poly_xy, 4, CLR_PINK);
    poly(poly_xy, 4, CLR_WHITE);

    // ── sprites (incl rotated), pal recolor, colorkey, map ─────────────────────────────────────
    spr(0, 6, 108);
    sprf(0, 26, 108, true, false);
    sspr(0, 0, 16, 16, 46, 108, 24, 24);                         // scaled blit
    spr_rot(0, 80, 116, deg);                                    // ROTATING sprite
    sspr_ex(0, 0, 16, 16, 110, 108, 24, 24, deg, 12, 12);        // ROTATING scaled
    pal(28, (f / 20) % 2 ? CLR_RED : CLR_GREEN); pal(29, CLR_YELLOW);
    spr(1, 150, 108);                                            // pal-recoloured person
    pal_reset();
    spr(2, 174, 108);                                            // colorkeyed (red bg gone)
    map_scale(1); map(0, 0, 200, 108, 4, 3);                     // the tiny tilemap

    // ── tritex — a rotating textured quad (2 tris) sampling slot 0 ─────────────────────────────
    { int cx = 284, cy = 132; float r = 16;
      int ax=(int)(cx+cosf(t)*r),       ay=(int)(cy+sinf(t)*r);
      int bx=(int)(cx+cosf(t+1.57f)*r), by=(int)(cy+sinf(t+1.57f)*r);
      int dx=(int)(cx+cosf(t+3.14f)*r), dy=(int)(cy+sinf(t+3.14f)*r);
      int ex=(int)(cx+cosf(t+4.71f)*r), ey=(int)(cy+sinf(t+4.71f)*r);
      tritex(ax,ay,0,0,  bx,by,16,0,  dx,dy,16,16);
      tritex(ax,ay,0,0,  dx,dy,16,16, ex,ey,0,16); }

    // ── text (incl rotated/scaled) ─────────────────────────────────────────────────────────────
    print("drawall", 6, 138, CLR_WHITE);
    font(FONT_SMALL); print("every draw cmd", 6, 148, CLR_LIGHT_GREY); font(FONT_NORMAL);
    print_outline("FX", 110, 140, CLR_YELLOW, CLR_DARK_RED);
    print_rot("spin", 150, 150, deg, CLR_GREEN, 1);             // ROTATING text
    print_rot_scaled("BIG", 200, 150, deg * 0.5f, 2, CLR_PINK, 1);   // ROTATING + scaled text

    // ── camera (translation), clip, zoom_rect ──────────────────────────────────────────────────
    camera(jit, 0); rectfill(6, 178, 12, 8, CLR_INDIGO); pset(6, 176, CLR_WHITE); camera(0, 0);
    clip(40, 172, 30, 22); rectfill(0, 160, 320, 40, CLR_DARK_GREEN); clip(0, 0, SCREEN_W, SCREEN_H);
    zoom_rect(0, 0, 36, 26, 240, 172, 72, 26);                  // magnify the top-left corner

    // ── reads (coverage only — pget/pget_rgb/sget; results intentionally unused) ────────────────
    (void)pget(40, 60); (void)pget_rgb(40, 60); (void)sget(2, 2);
}
