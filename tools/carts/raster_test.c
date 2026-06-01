#include "studio.h"

// Rasterization pixel-accuracy test cart.
//
// Z = toggle outline on/off
// X = toggle dithered fill on/off
//
// Draws several shapes, overlays a red outline, then detects mismatches:
//   Yellow = fill-edge pixel with no adjacent outline  (fill leaks past outline)
//   Pink   = outline pixel with no adjacent fill pixel (outline floats outside fill)
//
// watch("mismatches") reports the total mismatch pixel count each frame.
// Run headless via the debug harness and read the trace to assert 0 mismatches:
//   node tools/play.js raster_test script tools/raster_test.script \
//        --headless --trace build/raster_trace.jsonl --frames 8
//   cat build/raster_trace.jsonl | grep mismatches

#define BG        CLR_DARK_BLUE   // distinct background — not black, easy to spot leaks
#define FILL_A    CLR_WHITE        // dither color 0 (also solid fill color)
#define FILL_B    CLR_MEDIUM_GREY  // dither color 1 (two-color checker, not BG)
#define OUT_C     CLR_RED
#define M_FILL    CLR_YELLOW   // fill edge, no outline
#define M_OUT     CLR_PINK     // outline, no fill neighbour

static bool show_outline = true;
static bool show_dither  = false;

// a pixel counts as "fill" if it is either dither color
static bool is_fill(int c) { return c == FILL_A || c == FILL_B; }

static int count_mismatches(int x0, int y0, int x1, int y1) {
    int n = 0;
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            int c = pget(x, y);
            if (c == OUT_C) {
                bool has_fill = is_fill(pget(x-1,y)) || is_fill(pget(x+1,y)) ||
                                is_fill(pget(x,y-1)) || is_fill(pget(x,y+1));
                if (!has_fill) { pset(x, y, M_OUT); n++; }
            } else if (is_fill(c)) {
                bool on_edge = !is_fill(pget(x-1,y)) || !is_fill(pget(x+1,y)) ||
                               !is_fill(pget(x,y-1)) || !is_fill(pget(x,y+1));
                if (on_edge) {
                    bool has_out = show_outline &&
                                   (pget(x-1,y)==OUT_C || pget(x+1,y)==OUT_C ||
                                    pget(x,y-1)==OUT_C || pget(x,y+1)==OUT_C);
                    if (!has_out) { pset(x, y, M_FILL); n++; }
                }
            }
        }
    }
    return n;
}

static void draw_shape(int cx, int cy, int r) {
    if (show_dither) {
        fillp(FILL_CHECKER, FILL_B);  // two-color: FILL_A on 0-bits, FILL_B on 1-bits
        circfill(cx, cy, r, FILL_A);
        fillp_reset();
    } else {
        circfill(cx, cy, r, FILL_A);
    }
    if (show_outline) circ(cx, cy, r, OUT_C);
}

void update(void) {
    if (btnp(0, BTN_A)) show_outline = !show_outline;
    if (btnp(0, BTN_B)) show_dither  = !show_dither;
}

void draw(void) {
    cls(BG);

    // test shapes: circles at r=6, 10, 16, 22
    draw_shape( 16, 50,  6);
    draw_shape( 40, 50, 10);
    draw_shape( 74, 50, 16);
    draw_shape(118, 50, 22);

    // ovals
    if (show_dither) {
        fillp(FILL_CHECKER, FILL_B);
        ovalfill(20,  110, 14, 8,  FILL_A);
        ovalfill(70,  110, 20, 10, FILL_A);
        fillp_reset();
    } else {
        ovalfill(20,  110, 14, 8,  FILL_A);
        ovalfill(70,  110, 20, 10, FILL_A);
    }
    if (show_outline) {
        oval(20,  110, 14, 8, OUT_C);
        oval(70,  110, 20, 10, OUT_C);
    }

    // detect on ODD frames — pget reads the clean even-frame snapshot
    int mismatches = 0;
    if (frame() % 2 == 1) mismatches = count_mismatches(0, 0, 159, 159);

    watch("mismatches", "%d", mismatches);

    // HUD
    font(FONT_TINY);
    print(str("Z outline:%s  X dither:%s",
              show_outline ? "ON " : "OFF",
              show_dither  ? "ON " : "OFF"),
          2, 170, CLR_LIGHT_GREY);
    print(str("mismatches: %d", mismatches), 2, 178, mismatches > 0 ? M_FILL : CLR_LIME_GREEN);
    if (frame() % 2 == 1) {
        pset(158, 158, CLR_YELLOW);  // blink: detection active this frame
    }
    font(FONT_NORMAL);
}
