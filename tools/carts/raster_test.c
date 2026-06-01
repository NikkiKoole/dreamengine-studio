#include "studio.h"

// Rasterization pixel-accuracy test cart.
//
// Z     = toggle outline on/off
// X     = toggle dithered fill on/off
// SPACE = analyse (one-shot snapshot — press again to return to live)
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
#define HUD_H   16

static bool show_outline = true;
static bool show_dither  = false;

// freeze state machine
#define FS_LIVE     0   // drawing live
#define FS_SETUP    1   // draw clean once so pget captures it
#define FS_ANALYSE  2   // run analysis, paint markers
#define FS_FROZEN   3   // stay still, just repaint HUD
static int  fs          = FS_LIVE;
static int  last_count  = 0;

static bool is_fill(int c) { return c == FILL_A || c == FILL_B; }

static int analyse(void) {
    int n = 0;
    for (int y = 0; y < SCREEN_H - HUD_H; y++) {
        for (int x = 0; x < SCREEN_W; x++) {
            int c = pget(x, y);
            if (c == OUT_C) {
                // outline pixel floating outside fill — always wrong
                bool adj = is_fill(pget(x-1,y)) || is_fill(pget(x+1,y)) ||
                           is_fill(pget(x,y-1)) || is_fill(pget(x,y+1));
                if (!adj) { pset(x, y, M_OUT); n++; }
            } else if (show_outline && is_fill(c)) {
                // fill edge with no outline — only meaningful when outline is expected
                bool edge = !is_fill(pget(x-1,y)) || !is_fill(pget(x+1,y)) ||
                            !is_fill(pget(x,y-1)) || !is_fill(pget(x,y+1));
                if (edge) {
                    bool has_out = pget(x-1,y)==OUT_C || pget(x+1,y)==OUT_C ||
                                   pget(x,y-1)==OUT_C || pget(x,y+1)==OUT_C;
                    if (!has_out) { pset(x, y, M_FILL); n++; }
                }
            }
        }
    }
    return n;
}

static void draw_shapes(void) {
    cls(BG);
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

static void draw_hud(void) {
    rectfill(0, SCREEN_H - HUD_H, SCREEN_W, HUD_H, CLR_BLACK);
    font(FONT_TINY);
    print(str("Z outline:%s  X dither:%s  SPACE:analyse",
              show_outline ? "ON " : "OFF",
              show_dither  ? "ON " : "OFF"),
          2, SCREEN_H - 14, CLR_LIGHT_GREY);
    if (fs == FS_FROZEN || fs == FS_ANALYSE) {
        int col = last_count == 0 ? CLR_LIME_GREEN : M_FILL;
        print(str("mismatches: %d", last_count), 2, SCREEN_H - 6, col);
        if (fs == FS_FROZEN)
            print("frozen - SPACE to resume", SCREEN_W - 110, SCREEN_H - 6, CLR_INDIGO);
    }
    font(FONT_NORMAL);
#ifdef DE_TRACE
    // make the harness legible: state + last analysis result, every frame
    watch("state", "out=%d dith=%d fs=%d", show_outline, show_dither, fs);
    watch("mismatches", "%d", last_count);
#endif
}

void update(void) {
    if (btnp(0, BTN_A)) { show_outline = !show_outline; if (fs != FS_LIVE) fs = FS_SETUP; }
    if (btnp(0, BTN_B)) { show_dither  = !show_dither;  if (fs != FS_LIVE) fs = FS_SETUP; }
    if (keyp(KEY_SPACE)) fs = (fs == FS_LIVE) ? FS_SETUP : FS_LIVE;
}

void draw(void) {
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
