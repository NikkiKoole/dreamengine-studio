// placetest — correctness oracle for the window↔canvas transform (runtime/game_rect.h),
// the touch-controls Phase 1.5 chokepoint. PURE math, no raylib — runs anywhere.
//
// Asserts the property that retires the one hard-to-fix risk (silent, library-wide coordinate
// breakage when placement offsets/scales the game rect): the two transforms are exact mutual
// inverses, so a tap maps to the right game pixel at ANY placement. It covers not just the
// Phase-1.5 identity rect but offset+scaled rects (deck/rails-shaped), so it already guards
// Phase 2 — the bug would surface here, off-device, instead of on a phone.
//
//   ROUND TRIP   canvas -> window -> canvas == identity   (over a grid of sample points)
//   GR_PLACE     the Phase-1.5 stub returns a full-window identity placement
//
// Run: clang tools/det-probes/placetest.c -o /tmp/placetest && /tmp/placetest   (exit 0 = pass)
#include <stdio.h>

#define SCALE 4
#define SCREEN_W 320
#define SCREEN_H 200
#include "../../runtime/game_rect.h"

static int fails = 0;

// canvas pixel -> window -> canvas must return the same pixel for every sampled canvas point.
static void check_roundtrip(GameRect gr, const char *label) {
    int bad = 0;
    for (int cy = 0; cy <= SCREEN_H; cy += 7) {
        for (int cx = 0; cx <= SCREEN_W; cx += 11) {
            float wx = gr_canvas_to_win_x(gr, cx);
            float wy = gr_canvas_to_win_y(gr, cy);
            int rx = gr_win_to_canvas_x(gr, wx);
            int ry = gr_win_to_canvas_y(gr, wy);
            if (rx != cx || ry != cy) {
                if (bad < 4) printf("  [%s] MISMATCH canvas(%d,%d) -> win(%.2f,%.2f) -> canvas(%d,%d)\n",
                                    label, cx, cy, wx, wy, rx, ry);
                bad++;
            }
        }
    }
    if (bad) { printf("  [%s] %d round-trip mismatches\n", label, bad); fails++; }
    else     printf("  [%s] round-trip identity OK\n", label);
}

// assert gr_place picks the expected mode for a window size, and that its computed game rect
// round-trips. The window cases mirror the doc's matrix: matched / portrait / landscape.
static void check_place(int win_w, int win_h, PlaceMode want, const char *label) {
    Placement p = gr_place(win_w, win_h, SCREEN_W, SCREEN_H);
    if (p.mode != want) {
        printf("  [%s] win %dx%d: expected mode %d, got %d (game %.0f,%.0f x%.2f)\n",
               label, win_w, win_h, want, p.mode, p.game.x, p.game.y, p.game.scale);
        fails++;
    } else printf("  [%s] win %dx%d -> mode %d OK\n", label, win_w, win_h, p.mode);
    check_roundtrip(p.game, label);
}

int main(void) {
    // the placement decision matrix (doc's "what successful games do" cases):
    check_place(SCREEN_W * SCALE, SCREEN_H * SCALE, PLACE_OVERLAY, "matched (desktop)");  // game == window
    check_place(1080, 2400, PLACE_DECK,  "portrait phone");                               // tall band below
    check_place(2400, 1080, PLACE_RAILS, "landscape phone");                              // side rails
    check_place(1280,  816, PLACE_OVERLAY, "near-matched (tiny band)");                   // < GR_MIN_BAND → overlay

    // matched desktop must be an exact identity rect (the no-op guarantee).
    Placement d = gr_place(SCREEN_W * SCALE, SCREEN_H * SCALE, SCREEN_W, SCREEN_H);
    if (d.game.x != 0.0f || d.game.y != 0.0f || d.game.scale != (float)SCALE) {
        printf("  matched desktop not identity: game=(%.1f,%.1f,%.2f)\n", d.game.x, d.game.y, d.game.scale);
        fails++;
    } else printf("  matched desktop is exact identity OK\n");

    // round-trip on hand-built offset+scaled rects too (belt-and-suspenders for Phase-2 shapes).
    check_roundtrip((GameRect){ 0.0f, 0.0f, 2.0f }, "identity x2");
    check_roundtrip((GameRect){ 37.0f, 52.0f, 2.5f }, "offset frac (x2.5)");

    printf(fails ? "FAIL (%d)\n" : "PASS\n", fails);
    return fails ? 1 : 0;
}
