/* de:meta
{
  "title": "trifill stress",
  "status": "active",
  "created": "2026-06-02",
  "kind": [
    "tech-demo"
  ],
  "teaches": [
    "software-rasterizer"
  ],
  "lineage": "Sibling to polystress/tristress/clampstress — the first of the rasterizer regression rigs, specifically exposing the off-screen bbox clamp fix (2026-06-02) that brought a 12-spoke pinwheel from ~21fps to 60fps.",
  "description": "A spinning pinwheel of long, thin software-filled triangles — and a tiny lesson in why off-screen drawing used to cost you. Each spoke's far vertices sit way outside the 320x200 screen, so its bounding box is enormous while only a sliver is ever visible. The engine's CPU triangle filler used to scan that whole box, almost all of it off-screen and empty, which dragged a dozen spokes down to ~20fps — until the renderer learned to clamp the scan to the on-screen region. Now the off-screen reach is free (crank it to the max and it stays at a smooth 60fps); only on-screen coverage costs. A renderer regression test, not a game: it proves the clamp holds. The live fps counter turns yellow then red if it ever falls again. Up/Down add or remove spokes (on-screen cost), Left/Right grow or shrink the off-screen reach (now free), A freezes the spin to hold a steady frame."
}
de:meta */
#include "studio.h"

// ── trifill stress test — regression cart for the poly_fill_cov bbox clamp ──
//
// A pinwheel of long, THIN software-filled triangles. Each spoke's far vertices
// sit far outside the 320x200 screen, so its bounding box is enormous while only
// a sliver is ever visible.
//
// Before the off-screen bbox clamp (shipped 2026-06-02), studio.c's software
// rasterizer (poly_fill_cov) scanned the FULL bbox, calling poly_inside() on
// every cell — almost all off-screen, plotting nothing — which pinned this cart
// at ~21fps (46.7ms/frame). Now poly_fill_cov clamps its scan to the on-screen
// region (mapped through the camera), so the off-screen reach is FREE: it runs
// at a smooth 60fps (~2.7ms), and cranking RIGHT (reach) all the way to 8000 no
// longer costs anything. Cost now scales with on-screen coverage (spoke count),
// not off-screen reach.
//
// It survives as a regression cart: if the clamp ever breaks, pushing reach up
// will tank the fps again. The on-screen image is identical with or without the
// clamp (off-screen cells never plotted), so trifill() is the only heavy call —
// the profiler's draw-call count reads cleanly as "trifill ×N".
//
//   UP / DOWN     more / fewer spokes   (on-screen coverage — still costs)
//   RIGHT / LEFT  longer / shorter reach (off-screen — free since the clamp)
//   A             freeze rotation (hold one steady frame for the profiler)

#define CX 160
#define CY 100

static int   spokes = 12;        // thin triangles drawn per frame
static float reach  = 1100.0f;   // how far past the screen edge the far vertices go
static float wedge  = 2.2f;      // wedge width in degrees (thin = mostly-empty bbox)
static float ang    = 0;
static bool  spin   = true;

void update(void) {
    if (btnp(0, BTN_UP))   spokes += 2;
    if (btnp(0, BTN_DOWN)) spokes -= 2;
    spokes = mid(2, spokes, 80);

    if (btn(0, BTN_RIGHT)) reach += 50;
    if (btn(0, BTN_LEFT))  reach -= 50;
    reach = (float)mid(200, (int)reach, 8000);

    if (btnp(0, BTN_A)) spin = !spin;
    if (spin) ang += 24.0f * dt();      // degrees/sec
}

void draw(void) {
    cls(CLR_DARK_BLUE);

    // pinwheel: apex pinned at centre, two far vertices a hair apart and way
    // off-screen. Huge bbox, thin visible spoke.
    float step = 360.0f / spokes;
    for (int i = 0; i < spokes; i++) {
        float a   = ang + i * step;
        int   col = 8 + (i % 23);       // cycle palette indices 8..30
        int x1 = CX + (int)(cos_deg(a)         * reach);
        int y1 = CY + (int)(sin_deg(a)         * reach);
        int x2 = CX + (int)(cos_deg(a + wedge) * reach);
        int y2 = CY + (int)(sin_deg(a + wedge) * reach);
        trifill(CX, CY, x1, y1, x2, y2, col);
    }

    // ── read-out ──
    int f = fps();
    print(str("trifill stress  %d spokes  reach %d", spokes, (int)reach), 4, 4, CLR_WHITE);
    // skip the first frames: GetFPS() hasn't settled yet, so it would read a
    // misleadingly high number (this is also what the 3-frame thumbnail captures).
    if (frame() > 20)
        print(str("%d fps", f), 4, 14, f >= 55 ? CLR_LIME_GREEN : (f >= 30 ? CLR_YELLOW : CLR_RED));
    print("UP/DN spokes  L/R reach  A freeze", 4, SCREEN_H - 11, CLR_LIGHT_GREY);

#ifdef DE_TRACE
    watch("fps", "%d", f);
    watch("spokes", "%d", spokes);
    watch("reach", "%d", (int)reach);
#endif
}
