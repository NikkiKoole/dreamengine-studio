#include "studio.h"

// ---------------------------------------------------------------
// Rasterization consistency test cart
//
// PURPOSE: Make fill/outline/dither edge mismatches visible and
// measurable — pixel-perfect agreement is mandatory.
//
// LAYOUT:
//   Left strip (0..79):    actual-scale test cases
//   Right panel (80..319): 6× zoom of the region under the cursor
//
// TEST CASES (each drawn in a 28×28 cell in the left strip):
//   A – solid fill + outline (same color → outline must hug fill edge)
//   B – solid fill + outline (different colors → edge should be clean)
//   C – dithered fill + outline (dither must not speck past the outline)
//   D – adjacent solid circles (no crack, no bleed between them)
//   E – oval fill + outline
//
// MISMATCH DETECTION (runs every frame via pget on previous frame):
//   For each RED outline pixel in a test cell, flag it MAGENTA if it
//   has no fill-colored 4-neighbour → outline is floating outside fill.
//   For each fill edge pixel (fill pixel with an empty 4-neighbour),
//   flag it YELLOW if it has no adjacent outline pixel → fill leaks
//   past the outline.
// ---------------------------------------------------------------

#define ZOOM       6
#define ZOOM_W     (240 / ZOOM)   // 40 pixels wide
#define ZOOM_H     (200 / ZOOM)   // 33 pixels tall
#define STRIP_W    80
#define PANEL_X    80

// test cell layout in the left strip
#define CELL_W     28
#define CELL_H     28
#define CELL_COLS  2
#define CELL_ROWS  4

#define BG         CLR_BLACK
#define FILL_C     CLR_WHITE
#define OUTLINE_C  CLR_RED
#define DITH_FILL  CLR_LIGHT_GREY
#define MISMATCH_A CLR_YELLOW   // fill edge has no matching outline
#define MISMATCH_B CLR_PINK     // outline pixel has no fill neighbour

static int cell_x(int i) { return 2 + (i % CELL_COLS) * (CELL_W + 2); }
static int cell_y(int i) { return 2 + (i / CELL_COLS) * (CELL_H + 2); }

// draw one test case into its cell
static void draw_tests(void) {
    cls(BG);

    // label strip
    font(FONT_TINY);

    // --- A: solid circfill + circ outline, radius 11 ---
    {
        int cx = cell_x(0) + CELL_W/2, cy = cell_y(0) + CELL_H/2;
        circfill(cx, cy, 11, FILL_C);
        circ(cx, cy, 11, OUTLINE_C);
        print("A circ", cell_x(0), cell_y(0) - 6, CLR_LIGHT_GREY);
    }

    // --- B: solid circfill + circ outline, several radii ---
    {
        int bx = cell_x(1), by = cell_y(1);
        int radii[] = {3, 6, 9};
        int xs[]    = {bx+5, bx+14, bx+23};
        for (int i = 0; i < 3; i++) {
            circfill(xs[i], by + CELL_H/2, radii[i], FILL_C);
            circ(    xs[i], by + CELL_H/2, radii[i], OUTLINE_C);
        }
        print("B radii", bx, by - 6, CLR_LIGHT_GREY);
    }

    // --- C: dithered circfill + circ outline ---
    {
        int cx = cell_x(2) + CELL_W/2, cy = cell_y(2) + CELL_H/2;
        fillp(FILL_CHECKER, BG);
        circfill(cx, cy, 11, FILL_C);
        fillp_reset();
        circ(cx, cy, 11, OUTLINE_C);
        print("C dith", cell_x(2), cell_y(2) - 6, CLR_LIGHT_GREY);
    }

    // --- D: adjacent circles (crack test) ---
    {
        int bx = cell_x(3), by = cell_y(3);
        int cx1 = bx + 8,  cy1 = by + CELL_H/2;
        int cx2 = bx + 20, cy2 = by + CELL_H/2;
        circfill(cx1, cy1, 7, CLR_DARK_BLUE);
        circfill(cx2, cy2, 7, CLR_DARK_GREEN);
        circ(cx1, cy1, 7, CLR_BLUE);
        circ(cx2, cy2, 7, CLR_GREEN);
        print("D adj", bx, by - 6, CLR_LIGHT_GREY);
    }

    // --- E: ovalfill + oval outline ---
    {
        int cx = cell_x(4) + CELL_W/2, cy = cell_y(4) + CELL_H/2;
        ovalfill(cx, cy, 13, 8, FILL_C);
        oval(    cx, cy, 13, 8, OUTLINE_C);
        print("E oval", cell_x(4), cell_y(4) - 6, CLR_LIGHT_GREY);
    }

    // --- F: rrectfill + rrect outline ---
    {
        int bx = cell_x(5), by = cell_y(5);
        rrectfill(bx+2, by+4, CELL_W-4, CELL_H-8, 4, FILL_C);
        rrect(    bx+2, by+4, CELL_W-4, CELL_H-8, 4, OUTLINE_C);
        print("F rrec", bx, by - 6, CLR_LIGHT_GREY);
    }

    // --- G: dithered ovalfill + oval outline ---
    {
        int cx = cell_x(6) + CELL_W/2, cy = cell_y(6) + CELL_H/2;
        fillp(FILL_CHECKER, BG);
        ovalfill(cx, cy, 13, 8, FILL_C);
        fillp_reset();
        oval(cx, cy, 13, 8, OUTLINE_C);
        print("G doval", cell_x(6), cell_y(6) - 6, CLR_LIGHT_GREY);
    }

    font(FONT_NORMAL);
}

// analyse mismatches using pget on the previous frame
// highlights them in-place on the current frame
static void detect_mismatches(void) {
    // scan the entire left strip
    for (int y = 0; y < 200; y++) {
        for (int x = 0; x < STRIP_W; x++) {
            int c = pget(x, y);

            if (c == OUTLINE_C) {
                // outline pixel — check it has a fill neighbour
                bool has_fill = (pget(x-1,y) == FILL_C || pget(x+1,y) == FILL_C ||
                                 pget(x,y-1) == FILL_C || pget(x,y+1) == FILL_C);
                if (!has_fill)
                    pset(x, y, MISMATCH_B);  // outline floating outside fill
            }
            else if (c == FILL_C) {
                // fill pixel — if it is on the edge (has a non-fill neighbour)
                // it must have an adjacent outline pixel
                bool edge = (pget(x-1,y) != FILL_C || pget(x+1,y) != FILL_C ||
                             pget(x,y-1) != FILL_C || pget(x,y+1) != FILL_C);
                if (edge) {
                    bool has_outline = (pget(x-1,y) == OUTLINE_C || pget(x+1,y) == OUTLINE_C ||
                                        pget(x,y-1) == OUTLINE_C || pget(x,y+1) == OUTLINE_C);
                    if (!has_outline)
                        pset(x, y, MISMATCH_A);  // fill edge exposed, no outline
                }
            }
        }
    }
}

// draw the 6× zoom panel of the region under the mouse
static void draw_zoom(void) {
    int mx = mouse_x(), my = mouse_y();

    // center the zoom window on the cursor, clamped inside the strip
    int src_x = mx - ZOOM_W/2;
    int src_y = my - ZOOM_H/2;
    if (src_x < 0)          src_x = 0;
    if (src_y < 0)          src_y = 0;
    if (src_x + ZOOM_W > STRIP_W) src_x = STRIP_W - ZOOM_W;
    if (src_y + ZOOM_H > 200)     src_y = 200 - ZOOM_H;

    // black background for the zoom panel
    rectfill(PANEL_X, 0, 320 - PANEL_X, 200, CLR_DARKER_GREY);

    // magnify pixel by pixel
    for (int row = 0; row < ZOOM_H; row++) {
        for (int col = 0; col < ZOOM_W; col++) {
            int c = pget(src_x + col, src_y + row);
            int dx = PANEL_X + col * ZOOM;
            int dy = row * ZOOM;
            rectfill(dx, dy, ZOOM, ZOOM, c);
            // 1px grid
            rect(dx, dy, ZOOM, ZOOM, CLR_DARKER_GREY);
        }
    }

    // cross-hair at the cursor pixel in the zoom
    int cur_col = mx - src_x, cur_row = my - src_y;
    rect(PANEL_X + cur_col * ZOOM - 1, cur_row * ZOOM - 1, ZOOM + 2, ZOOM + 2, CLR_YELLOW);

    // legend
    font(FONT_TINY);
    int lx = PANEL_X + 2, ly = 200 - 22;
    rectfill(lx-1, ly-1, 115, 23, CLR_BLACK);
    pset(lx+2, ly+2, MISMATCH_A); pset(lx+3,ly+2,MISMATCH_A);
    print("fill edge, no outline", lx+6, ly,   MISMATCH_A);
    pset(lx+2, ly+9, MISMATCH_B); pset(lx+3,ly+9,MISMATCH_B);
    print("outline, no fill nbr", lx+6, ly+8, MISMATCH_B);
    font(FONT_NORMAL);
}

void draw(void) {
    draw_tests();
    detect_mismatches();
    draw_zoom();
}
