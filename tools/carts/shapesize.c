/* de:meta
{
  "slug": "shapesize",
  "title": "shape size probe",
  "status": "active",
  "created": "2026-07-17",
  "kind": ["tech-demo"],
  "teaches": ["algorithm-visualization"],
  "lineage": "Size/extent twin of raster_test: where raster_test checks each fill's outline lands on its own boundary, this measures each shape's absolute rendered bounding box (via pget) against the width/height we expect, catching off-by-one edge bugs like the 2026-07 rrectfill 1px-short fix.",
  "description": "A renderer size test bench, not a game. It cycles through every fill primitive (rect/rrect/circ/oval/tri/quad/poly/ngon/star/arc/ring, plus outlines), draws each one white, measures its actual bounding box with pget, and prints the measured width x height next to the size we expect. Corner shapes (rect/rrect) must be EXACT; center-radius shapes (circle/oval/ngon/star) report their 2r extent + a left/right/top/bottom symmetry read; vertex shapes (tri/quad/poly) show how the top-left fill rule trims the far edge. Under -DDE_TRACE it emits per-shape extents on the trace for tools/play.js to gate."
}
de:meta */
#include "studio.h"

// One shape per ~0.5s window (held, so a human can read it; pget has no frame-lag
// while the scene is static). Under -DDE_TRACE every frame emits the measured bbox:
//   node tools/play.js shapesize script /dev/null --headless --frames 240 --trace out.jsonl
// then dedupe by "nm" (any settled frame per shape is identical).

#define HOLD 15          // frames each shape is shown
#define NSHAPE 16
#define SHC CLR_WHITE     // shapes drawn in white — measured
#define TXC CLR_MEDIUM_GREY  // labels in grey — excluded from the measure

void init(void) { enable_pget(true); }

// bbox of WHITE pixels only (labels are grey, so they don't pollute the size)
static int mnx, mxx, mny, mxy, hits;
static void measure(void) {
    mnx = SCREEN_W; mny = SCREEN_H; mxx = -1; mxy = -1; hits = 0;
    for (int y = 0; y < SCREEN_H; y++)
        for (int x = 0; x < SCREEN_W; x++)
            if (pget(x, y) == SHC) {
                hits++;
                if (x < mnx) mnx = x; if (x > mxx) mxx = x;
                if (y < mny) mny = y; if (y > mxy) mxy = y;
            }
}
#define MW (mxx - mnx + 1)
#define MH (mxy - mny + 1)

static const char *name_of(int i) {
    return i==0?"rectfill":i==1?"rrectfill":i==2?"rrectfill_r0":i==3?"trifill":
           i==4?"quadfill":i==5?"polyfill":i==6?"circfill":i==7?"ovalfill":
           i==8?"arcfill360":i==9?"ring360":i==10?"ngonfill6":i==11?"starfill5":
           i==12?"rect":i==13?"rrect":i==14?"circ":"ngon6";
}

static void draw_shape(int i) {
    switch (i) {
    case 0:  rectfill(30, 40, 50, 40, SHC); break;
    case 1:  rrectfill(30, 40, 50, 40, 8, SHC); break;
    case 2:  rrectfill(30, 40, 50, 40, 0, SHC); break;
    case 3:  trifill(20, 30, 90, 55, 40, 90, SHC); break;
    case 4:  quadfill(20, 30, 85, 35, 80, 88, 25, 90, SHC); break;
    case 5:  { int p[] = {50,25, 90,55, 75,95, 25,95, 10,55}; polyfill(p, 5, SHC); } break;
    case 6:  circfill(80, 60, 20, SHC); break;
    case 7:  ovalfill(80, 60, 30, 15, SHC); break;
    case 8:  arcfill(80, 60, 20, 0, 360, SHC); break;
    case 9:  ring(80, 60, 0, 20, 0, 360, SHC); break;
    case 10: ngonfill(80, 60, 25, 6, 0, SHC); break;
    case 11: starfill(80, 60, 28, 12, 5, -90, SHC); break;
    case 12: rect(30, 40, 50, 40, SHC); break;
    case 13: rrect(30, 40, 50, 40, 8, SHC); break;
    case 14: circ(80, 60, 20, SHC); break;
    case 15: ngon(80, 60, 25, 6, 0, SHC); break;
    }
}

// expected EXACT bbox for the strict (corner + vertex) shapes; ex1<0 = measure-only
static void expected(int i, int *ex0, int *ey0, int *ex1, int *ey1) {
    *ex0 = 0; *ey0 = 0; *ex1 = -1; *ey1 = -1;
    switch (i) {
    case 0: case 2: case 12: case 1: case 13: *ex0=30; *ey0=40; *ex1=79; *ey1=79; break;
    case 3:  *ex0=20; *ey0=30; *ex1=90; *ey1=90; break;   // tri verts min/max
    case 4:  *ex0=20; *ey0=30; *ex1=85; *ey1=90; break;   // quad
    case 5:  *ex0=10; *ey0=25; *ex1=90; *ey1=95; break;   // poly
    }
}

void draw(void) {
    cls(CLR_BLACK);
    int f = (int)(now() * 60.0f + 0.5f);
    int i = (f / HOLD) % NSHAPE;
    draw_shape(i);

    // legible labels for a human opening the cart
    font(FONT_SMALL);
    print("SHAPE SIZE PROBE", 4, 4, TXC);
    print(name_of(i), 4, SCREEN_H - 24, TXC);   // grey → excluded from the white-only measure

#ifdef DE_TRACE
    measure();
    if (hits == 0) return;   // window not settled yet
    int ex0, ey0, ex1, ey1; expected(i, &ex0, &ey0, &ex1, &ey1);
    watch("nm", "%s", name_of(i));
    watch("bbox", "%d,%d..%d,%d", mnx, mny, mxx, mxy);
    watch("w", "%d", MW); watch("h", "%d", MH);
    if (ex1 >= 0) {
        int ok = (mnx==ex0 && mny==ey0 && mxx==ex1 && mxy==ey1);
        watch("exp", "%d,%d..%d,%d", ex0, ey0, ex1, ey1);
        watch("ok", "%d", ok);
    } else {   // center shapes: symmetry about (80,60)
        watch("lg", "%d", 80 - mnx); watch("rg", "%d", mxx - 80);
        watch("tg", "%d", 60 - mny); watch("bg", "%d", mxy - 60);
    }
#else
    // human view: show the measured size live too
    font(FONT_SMALL);
    print("cycles every fill primitive; open the trace for exact extents", 4, SCREEN_H - 12, TXC);
#endif
}
