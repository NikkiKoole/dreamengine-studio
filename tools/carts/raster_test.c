/* de:meta
{
  "slug": "raster_test",
  "title": "rasterizer test",
  "status": "active",
  "created": "2026-06-01",
  "kind": [
    "tech-demo"
  ],
  "teaches": [
    "algorithm-visualization",
    "dithering-gradient"
  ],
  "lineage": "Internal engine QA tool; uses a two-frame pget() readback pass to verify fill/outline pixel invariants, marking failures pink/yellow — the analysis loop itself is the demo.",
  "description": "A renderer pixel-accuracy test bench, not a game — it proves every fill primitive's outline lands exactly on its fill boundary. Four pages of shapes (curved: circ/oval/rect/rrect; polygons: tri/quad/ngon/star; arcs: arcfill/ring/thickline; plus an equivalence self-test), each drawn as a white fill with a red outline. Press SPACE to analyse: it reads the canvas back with pget and flags any outline pixel buried inside the fill (pink) or any fill edge the outline failed to cover (yellow), reporting a live mismatch count that should read 0. Z toggles outlines, X toggles dither fill, C cycles pages."
}
de:meta */
#include "studio.h"
#include <math.h>

// Rasterization pixel-accuracy test cart.
//
// Z     = toggle outline on/off
// X     = toggle dithered fill on/off
// C     = cycle page (1 = curved: rect/circ/oval/rrect, 2 = poly: tri/quad/ngon/star/poly,
//                     3 = arcs: arcfill vs circ + arc/ring/thickline demo,
//                     4 = equivalence self-test: ring/sector/thickline vs reference)
// SPACE = analyse (pages 1-3) / run equivalence checks (page 4)
//
// Live mode: clean shapes, no flicker
// After SPACE:
//   frame 0: draw clean shapes so pget captures them
//   frame 1: pget reads clean frame → run analysis → paint markers on canvas
//   frame 2+: canvas stays frozen (no cls) — markers remain visible
//             HUD redrawn each frame to stay crisp
//
// Yellow = fill edge pixel with no adjacent outline
// Pink   = outline pixel with no adjacent fill

#define BG      CLR_DARK_BLUE
#define FILL_A  CLR_WHITE
#define FILL_B  CLR_MEDIUM_GREY
#define OUT_C   CLR_RED
#define M_FILL  CLR_YELLOW
#define M_OUT   CLR_PINK
#define DEMO_C  CLR_ORANGE   // open strokes: drawn for the eye, ignored by analyse()
#define HUD_H   16
#define NPAGES  4

static bool show_outline = true;
static bool show_dither  = false;
static int  page         = 0;

// freeze state machine
#define FS_LIVE     0   // drawing live
#define FS_SETUP    1   // draw clean once so pget captures it
#define FS_ANALYSE  2   // run analysis, paint markers
#define FS_FROZEN   3   // stay still, just repaint HUD
static int  fs          = FS_LIVE;
static int  last_count  = 0;
static float quad_rot   = 0;   // page-2 rotating quad — spins in LIVE, frozen the instant SPACE analyses (so pget'd SETUP frame == ANALYSE redraw)

static bool is_fill(int c) { return c == FILL_A || c == FILL_B; }
// the whole shape = fill ∪ outline ∪ already-painted markers (markers only ever
// replace pixels that were solid, so counting them keeps the region intact and
// stops the in-place painting from feeding back into later neighbour reads).
static bool is_solid(int c) { return is_fill(c) || c == OUT_C || c == M_FILL || c == M_OUT; }

// The invariant: the outline must be EXACTLY the boundary of the fill region.
// Reconstruct the region from the single render (fill ∪ outline) and check:
//   - any OUTLINE pixel with no background neighbour is buried inside the fill
//     (the outline strayed off the true edge)            → pink
//   - any FILL pixel that itself touches background was not covered by the
//     outline (a gap / the outline pulled away from it)  → yellow
// This is global, not a local guess, so it catches a 1px offset at any angle
// yet never false-flags a sharp tip (a correct tip pixel IS on the boundary).
static int n_pink = 0, n_yellow = 0;
static int analyse(void) {
    int n = 0; n_pink = 0; n_yellow = 0;
    for (int y = 0; y < SCREEN_H - HUD_H; y++) {
        for (int x = 0; x < SCREEN_W; x++) {
            int c = pget(x, y);
            bool touches_bg = !is_solid(pget(x-1,y)) || !is_solid(pget(x+1,y)) ||
                              !is_solid(pget(x,y-1)) || !is_solid(pget(x,y+1));
            if (c == OUT_C) {
                if (!touches_bg) { pset(x, y, M_OUT); n++; n_pink++; }       // buried outline
            } else if (show_outline && is_fill(c)) {
                if (touches_bg) { pset(x, y, M_FILL); n++; n_yellow++; }     // uncovered fill edge
            }
        }
    }
    return n;
}

// ===== equivalence self-tests (page 4) ==========================================
// Open strokes (ring / arcfill sector / thickline) have no engine outline, so the
// outline==fill detector can't score them. Instead verify each by comparing its
// pixel set to an INDEPENDENTLY rasterized reference, or to a solidity property:
//   T0 ring(…,0,360)            == circfill(ro) minus circfill(ri)   [sector_fill vs disc_inside]
//   T1 arcfill(0,180)+(180,360) == circfill                          [angular seam — no crack/overlap]
//   T2 thickline                 has no enclosed background           [cap/body merge is solid]
// pget reads the PRIOR frame's snapshot, so reference and test live on consecutive
// frames: START draws ref; CAPTURE (snapshot=ref) stores it + draws test; COMPARE (snapshot=test) scores.
#define EQ_N      3
#define EQ_IDLE   0
#define EQ_START  1
#define EQ_CAP    2
#define EQ_CMP    3
static unsigned char refbuf[SCREEN_W * SCREEN_H];
static int  eq_step = EQ_IDLE, eq_idx = 0, eq_diff[EQ_N];
static bool eq_ran  = false;

static void eq_ref(int t) {     // independent reference, drawn in FILL_A
    cls(BG);
    if      (t == 0) { circfill(60, 78, 34, FILL_A); circfill(60, 78, 16, BG); }  // annulus
    else if (t == 1) { circfill(60, 78, 30, FILL_A); }                            // full disc
    // t == 2: no reference (property test)
}
static void eq_test_shape(int t) {
    cls(BG);
    if      (t == 0) ring(60, 78, 16, 34, 0, 360, FILL_A);
    else if (t == 1) { arcfill(60,78,30, 0,180, FILL_A); arcfill(60,78,30, 180,360, FILL_A); }
    else             thickline(30, 40, 290, 150, 11, FILL_A);
}
static int eq_compare(int t) {  // pget now reads the test shape's snapshot
    int n = 0;
    if (t < 2) {                                       // pixel-set equality vs refbuf
        for (int y = 0; y < SCREEN_H - HUD_H; y++)
            for (int x = 0; x < SCREEN_W; x++)
                if ((refbuf[y*SCREEN_W + x] != 0) != (pget(x,y) == FILL_A)) n++;
    } else {                                           // thickline: enclosed bg = crack/hole
        for (int y = 1; y < SCREEN_H - HUD_H - 1; y++)
            for (int x = 1; x < SCREEN_W - 1; x++)
                if (pget(x,y) != FILL_A &&
                    ((pget(x-1,y)==FILL_A && pget(x+1,y)==FILL_A) ||
                     (pget(x,y-1)==FILL_A && pget(x,y+1)==FILL_A))) {
                    n++;
#ifdef DE_TRACE
                    if (n <= 4) watch(str("crack%d", n), "x=%d y=%d", x, y);
#endif
                }
    }
    return n;
}
static void eq_capture_ref(void) {
    for (int y = 0; y < SCREEN_H - HUD_H; y++)
        for (int x = 0; x < SCREEN_W; x++)
            refbuf[y*SCREEN_W + x] = (pget(x,y) == FILL_A);
}

// page 1 — curved primitives (unified on pixel-center coverage)
static void draw_page1(void) {
    int cxs[] = {16, 40, 74, 118};
    int crs[] = {6, 10, 16, 22};
    for (int i = 0; i < 4; i++) {
        if (show_dither) fillp(FILL_CHECKER, FILL_B);
        circfill(cxs[i], 50, crs[i], FILL_A);
        if (show_dither) fillp_reset();
        if (show_outline) circ(cxs[i], 50, crs[i], OUT_C);
    }
    if (show_dither) fillp(FILL_CHECKER, FILL_B);
    ovalfill(20, 115, 14, 8,  FILL_A);
    ovalfill(70, 115, 20, 10, FILL_A);
    if (show_dither) fillp_reset();
    if (show_outline) {
        oval(20, 115, 14, 8,  OUT_C);
        oval(70, 115, 20, 10, OUT_C);
    }
    // plain rects — axis-aligned; rect border must equal the rectfill boundary
    int qx[] = {98, 98};
    int qy[] = {86, 116};
    int qw[] = {54, 46};
    int qh[] = {24, 20};
    for (int i = 0; i < 2; i++) {
        if (show_dither) fillp(FILL_CHECKER, FILL_B);
        rectfill(qx[i], qy[i], qw[i], qh[i], FILL_A);
        if (show_dither) fillp_reset();
        if (show_outline) rect(qx[i], qy[i], qw[i], qh[i], OUT_C);
    }
    // rounded rects — varied corner radii (small, big, near-stadium)
    int rx[] = {170, 235, 170, 250, 20, 115};
    int ry[] = { 28,  28,  90,  90, 140, 140};
    int rw[] = { 50,  70,  60,  55,  80,  90};
    int rh[] = { 40,  50,  34,  55,  36,  38};
    int rr[] = { 10,  18,   6,  24,  12,   4};
    for (int i = 0; i < 6; i++) {
        if (show_dither) fillp(FILL_CHECKER, FILL_B);
        rrectfill(rx[i], ry[i], rw[i], rh[i], rr[i], FILL_A);
        if (show_dither) fillp_reset();
        if (show_outline) rrect(rx[i], ry[i], rw[i], rh[i], rr[i], OUT_C);
    }
}

// page 2 — polygon primitives (still on the old GPU/fan paths — under test)
static void draw_page2(void) {
    // triangles — tri/trifill now share the polygon coverage path
    int tris[][6] = {
        { 15,28,  58,40,  30,76},
        { 72,76, 118,28, 128,72},
        {140,30, 195,34, 170,78},
    };
    for (int i = 0; i < 3; i++) {
        int *t = tris[i];
        if (show_dither) fillp(FILL_CHECKER, FILL_B);
        trifill(t[0],t[1], t[2],t[3], t[4],t[5], FILL_A);
        if (show_dither) fillp_reset();
        if (show_outline) tri(t[0],t[1], t[2],t[3], t[4],t[5], OUT_C);
    }
    // regular polygons: pentagon, hexagon, octagon
    int   gx[]   = {32, 85, 142};
    int   gr[]   = {18, 20,  18};
    int   gs[]   = { 5,  6,   8};
    float grot[] = {-90, 0,  22};
    for (int i = 0; i < 3; i++) {
        if (show_dither) fillp(FILL_CHECKER, FILL_B);
        ngonfill(gx[i], 122, gr[i], gs[i], grot[i], FILL_A);
        if (show_dither) fillp_reset();
        if (show_outline) ngon(gx[i], 122, gr[i], gs[i], grot[i], OUT_C);
    }
    // stars: 5-point (top right), 6-point (mid)
    if (show_dither) fillp(FILL_CHECKER, FILL_B);
    starfill(250,  52, 24, 10, 5, -90, FILL_A);
    starfill(205, 122, 20,  9, 6,   0, FILL_A);
    if (show_dither) fillp_reset();
    if (show_outline) {
        star(250,  52, 24, 10, 5, -90, OUT_C);
        star(205, 122, 20,  9, 6,   0, OUT_C);
    }
    // ROTATING oriented box — the regime an axis-aligned quad never reaches. A rotated box is
    // exactly what boxjelly draws for its spinner/pinwheel crates, and the class of bug that hid
    // there: pairing a COVERAGE fill (polyfill) with a DDA outline (line()) drifts up to a pixel
    // apart on rotated edges in the software canvas (HW hides it). The correct pairing is
    // polyfill + poly (poly()'s stroke IS the boundary ring of polyfill's coverage), so this must
    // read 0 at EVERY angle. Spins in LIVE; SPACE freezes the current angle and analyses it.
    {
        float a = quad_rot * 0.017453293f, ca = cosf(a), sa = sinf(a);
        int cx = 283, cy = 124; float hw = 28, hh = 18;
        float lx[4] = {-hw, hw, hw, -hw}, ly[4] = {-hh, -hh, hh, hh}; int quad[8];
        for (int i = 0; i < 4; i++) {
            quad[i*2]   = cx + (int)roundf(ca*lx[i] - sa*ly[i]);
            quad[i*2+1] = cy + (int)roundf(sa*lx[i] + ca*ly[i]);
        }
        if (show_dither) fillp(FILL_CHECKER, FILL_B);
        polyfill(quad, 4, FILL_A);
        if (show_dither) fillp_reset();
        if (show_outline) poly(quad, 4, OUT_C);
    }
    // quadfill — two trifills sharing a diagonal; poly() of the same 4 corners is
    // the boundary, so 0 mismatches proves the shared diagonal is watertight.
    int qd[] = {282,30, 316,42, 309,74, 285,64};
    if (show_dither) fillp(FILL_CHECKER, FILL_B);
    quadfill(qd[0],qd[1], qd[2],qd[3], qd[4],qd[5], qd[6],qd[7], FILL_A);
    if (show_dither) fillp_reset();
    if (show_outline) poly(qd, 4, OUT_C);
}

// page 3 — arc family + thick strokes
static void draw_page3(void) {
    // AUTO-VERIFIED: a full-circle arcfill must paint the same disc as circfill,
    // so circ (the circle's boundary ring) is exactly the boundary of the arcfill
    // disc → 0 mismatches iff arcfill agrees with the unified circle definition.
    int ax[] = {28, 74, 132, 196};
    int ar[] = {12, 18,  24,  16};
    for (int i = 0; i < 4; i++) {
        if (show_dither) fillp(FILL_CHECKER, FILL_B);
        arcfill(ax[i], 52, ar[i], 0, 360, FILL_A);
        if (show_dither) fillp_reset();
        if (show_outline) circ(ax[i], 52, ar[i], OUT_C);
    }
    // open strokes — now fill + boundary-ring outline (arcoutline/ringoutline/
    // thicklineoutline), so they take a pattern and the detector grades them too.
    // pie sector
    if (show_dither) fillp(FILL_CHECKER, FILL_B);
    arcfill(36, 128, 28, 205, 340, FILL_A);
    if (show_dither) fillp_reset();
    if (show_outline) arcoutline(36, 128, 28, 205, 340, OUT_C);
    // full ring / annulus
    if (show_dither) fillp(FILL_CHECKER, FILL_B);
    ring(108, 128, 12, 24, 0, 360, FILL_A);
    if (show_dither) fillp_reset();
    if (show_outline) ringoutline(108, 128, 12, 24, 0, 360, OUT_C);
    // thick line
    if (show_dither) fillp(FILL_CHECKER, FILL_B);
    thickline(166, 108, 214, 152, 11, FILL_A);
    if (show_dither) fillp_reset();
    if (show_outline) thicklineoutline(166, 108, 214, 152, 11, OUT_C);
    // partial ring (gauge)
    if (show_dither) fillp(FILL_CHECKER, FILL_B);
    ring(272, 130, 14, 26, 200, 345, FILL_A);
    if (show_dither) fillp_reset();
    if (show_outline) ringoutline(272, 130, 14, 26, 200, 345, OUT_C);
}

static void draw_shapes(void) {
    cls(BG);
    if      (page == 0) draw_page1();
    else if (page == 1) draw_page2();
    else                draw_page3();
}

static void draw_hud(void) {
    rectfill(0, SCREEN_H - HUD_H, SCREEN_W, HUD_H, CLR_BLACK);
    font(FONT_TINY);
    print(str("Z out:%s  X dith:%s  C page %d/%d  SPACE:analyse",
              show_outline ? "ON " : "OFF",
              show_dither  ? "ON " : "OFF",
              page + 1, NPAGES),
          2, SCREEN_H - 14, CLR_LIGHT_GREY);
    if (fs == FS_FROZEN || fs == FS_ANALYSE) {
        int col = last_count == 0 ? CLR_LIME_GREEN : M_FILL;
        print(str("mismatches: %d", last_count), 2, SCREEN_H - 6, col);
        if (fs == FS_FROZEN)
            print("frozen - SPACE to resume", SCREEN_W - 110, SCREEN_H - 6, CLR_INDIGO);
    }
    font(FONT_NORMAL);
#ifdef DE_TRACE
    // make the harness legible: page + state + last analysis result, every frame
    watch("state", "pg=%d out=%d dith=%d fs=%d", page + 1, show_outline, show_dither, fs);
    watch("mismatches", "%d", last_count);
    watch("split", "pink=%d yellow=%d", n_pink, n_yellow);
#endif
}

#define EQ_PAGE 3   // page index 3 = equivalence self-test

static void draw_equiv_hud(void) {
    rectfill(0, SCREEN_H - HUD_H, SCREEN_W, HUD_H, CLR_BLACK);
    font(FONT_TINY);
    print(str("EQUIV self-test  C page %d/%d  SPACE:run", page + 1, NPAGES),
          2, SCREEN_H - 14, CLR_LIGHT_GREY);
    if (eq_ran) {
        int tot = eq_diff[0] + eq_diff[1] + eq_diff[2];
        print(str("ring:%d  sector:%d  thickline:%d", eq_diff[0], eq_diff[1], eq_diff[2]),
              2, SCREEN_H - 6, tot == 0 ? CLR_LIME_GREEN : M_FILL);
    } else if (eq_step != EQ_IDLE) {
        print(str("running test %d/%d...", eq_idx + 1, EQ_N), 2, SCREEN_H - 6, CLR_INDIGO);
    } else {
        print("SPACE to run", 2, SCREEN_H - 6, CLR_INDIGO);
    }
    font(FONT_NORMAL);
#ifdef DE_TRACE
    watch("state", "pg=%d eq_step=%d eq_idx=%d ran=%d", page + 1, eq_step, eq_idx, eq_ran);
    watch("eq", "ring=%d sector=%d thick=%d total=%d", eq_diff[0], eq_diff[1], eq_diff[2],
          eq_diff[0] + eq_diff[1] + eq_diff[2]);
#endif
}

// drives the 3-frame-per-test sequence; pget reads the prior frame's snapshot
static void draw_equiv(void) {
    if (eq_step == EQ_START) {              // draw reference for current test
        eq_ref(eq_idx);
        eq_step = EQ_CAP;
    } else if (eq_step == EQ_CAP) {         // snapshot = reference: store it, draw test
        if (eq_idx < 2) eq_capture_ref();
        eq_test_shape(eq_idx);
        eq_step = EQ_CMP;
    } else if (eq_step == EQ_CMP) {         // snapshot = test: score, advance
        eq_diff[eq_idx] = eq_compare(eq_idx);
        if (++eq_idx < EQ_N) { eq_ref(eq_idx); eq_step = EQ_CAP; }
        else { eq_step = EQ_IDLE; eq_ran = true; }
    } else {                                // EQ_IDLE: show the test shapes for the eye
        cls(BG);
        ring(56, 70, 16, 30, 0, 360, DEMO_C);
        arcfill(150, 70, 28, 0, 180, DEMO_C); arcfill(150, 70, 28, 180, 360, DEMO_C);
        thickline(210, 40, 300, 120, 11, DEMO_C);
    }
    draw_equiv_hud();
}

void init(void) { enable_pget(true); }   // this cart reads the canvas back in its analyse pass

void update(void) {
    if (btnp(0, BTN_A)) { show_outline = !show_outline; if (fs != FS_LIVE) fs = FS_SETUP; }
    if (btnp(0, BTN_B)) { show_dither  = !show_dither;  if (fs != FS_LIVE) fs = FS_SETUP; }
    if (keyp('C'))      { page = (page + 1) % NPAGES;   if (fs != FS_LIVE) fs = FS_SETUP; }
    if (keyp(KEY_SPACE)) {
        if (page == EQ_PAGE) { eq_idx = 0; eq_step = EQ_START; eq_ran = false; }
        else fs = (fs == FS_LIVE) ? FS_SETUP : FS_LIVE;
    }
    if (fs == FS_LIVE) quad_rot += 1.7f;   // advance only while live → angle is frozen across SETUP+ANALYSE
}

void draw(void) {
    if (page == EQ_PAGE) { draw_equiv(); return; }
    if (fs == FS_LIVE) {
        draw_shapes();
        draw_hud();
        return;
    }
    if (fs == FS_SETUP) {
        // draw clean — pget will capture this next frame
        draw_shapes();
        draw_hud();
        fs = FS_ANALYSE;
        return;
    }
    if (fs == FS_ANALYSE) {
        // pget reads the clean FS_SETUP frame — run analysis, paint markers
        draw_shapes();
        last_count = analyse();
        draw_hud();
        fs = FS_FROZEN;
        return;
    }
    // FS_FROZEN: canvas keeps the markers — just repaint HUD on top
    draw_hud();
}
