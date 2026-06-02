#include "studio.h"

// ── trifill stress test — measures the "clamp poly_fill_cov bbox to screen" win ──
//
// A pinwheel of long, THIN software-filled triangles. Each spoke's far vertices
// sit far outside the 320x200 screen, so its bounding box is enormous while only
// a sliver is ever visible. studio.c's software rasterizer (poly_fill_cov) today
// iterates the FULL bbox, calling poly_inside() on every cell — the overwhelming
// majority off-screen, plotting nothing. That wasted scan is what STATUS open
// item 14 ("clamp poly_fill_cov's scan bbox to the screen") removes.
//
// This cart exists to make that waste measurable and the fix verifiable:
//   • before/after the engine clamp, the on-screen image must stay pixel-identical
//     (the off-screen cells never plotted anything anyway), while the profiler's
//     CPU frame-budget line drops sharply.
//   • trifill() is the only heavy call, so the profiler's draw-call count reads
//     cleanly as "trifill ×N".
//
// It is intentionally over budget at the defaults so the win is obvious. Tune live:
//   UP / DOWN     more / fewer spokes   (raises/lowers the bbox count)
//   RIGHT / LEFT  longer / shorter reach (how far off-screen the bbox extends)
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
