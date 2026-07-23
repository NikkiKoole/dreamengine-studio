/* de:meta
title: Tiny Acid Jam — App Icon
slug: acidicon
kind: [toy]
teaches: [layout]
created: 2026-07-22
description:
  summary: The Tiny Acid Jam app icon — the rack's own material in a 128px field.
  detail: >
    Renders the App Store icon for Tiny Acid Jam from the app's own parts: two 303 pitch-
    ladders (note bars + slide glides), an 808 cell strip and a 909 cell strip, plus the acid
    CUT knob as the hero. Full-bleed dark field, the EXACT device colours the maker tuned
    (303a orange / 303b coral / 808 green / 909 yellow — raw RGB via rectfill_rgb, not palette).
    Honestly pixel-made, not an AI illustration. Draw at 128px, integer-upscale x8 to 1024.
      node tools/play.js acidicon run --headless --screen 128x128 --frames 1 --dump build/.icon
      ffmpeg -y -i build/.icon/frame_00000.png -vf scale=1024:1024:flags=neighbor -pix_fmt rgb24 icon.png
  controls: none — a static render.
*/
#include "studio.h"
#include <math.h>
#include <stdlib.h>

// the maker's exact device colours (see acidcandy.c RWASH_RGB)
#define C_303A 0xf69c31
#define C_303B 0xf26c55
#define C_808  0x4fd94a
#define C_909  0xfee74b
#define C_BG   0x171019
#define C_CELL 0x2a2433   // an unlit drum cell
#define C_INK  0xfff1e8   // the knob pointer / accent pips

// ── tiny raw-RGB helpers (the engine only ships rectfill_rgb / pset_rgb) ──
static void line_rgb(int x0, int y0, int x1, int y1, int hex) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1, err = dx + dy;
    for (;;) { pset_rgb(x0, y0, hex);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err; if (e2 >= dy) { err += dy; x0 += sx; } if (e2 <= dx) { err += dx; y0 += sy; } }
}
static void disc_rgb(int cx, int cy, int r, int hex) {
    for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++)
        if (dx * dx + dy * dy <= r * r) pset_rgb(cx + dx, cy + dy, hex);
}
static void ring_rgb(int cx, int cy, int r, int t, int hex) {
    int ro = r * r, ri = (r - t) * (r - t);
    for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++) {
        int d = dx * dx + dy * dy; if (d <= ro && d >= ri) pset_rgb(cx + dx, cy + dy, hex); }
}

// a 303 pitch-ladder: a note bar per step at its pitch height, slide glides linking held steps
static void ladder(int x, int y, int w, int h, int hex, const int *pit, const int *sld) {
    int N = 8;
    int prevcx = 0, prevcy = 0, prevon = 0;
    for (int i = 0; i < N; i++) {
        int bx = x + i * w / N, bw = (x + (i + 1) * w / N) - bx - 1;
        int ny = y + h - 4 - pit[i] * ((h - 8) / 5);   // higher pitch = higher bar
        rectfill_rgb(bx, ny, bw, 3, hex);              // the note bar
        int cx = bx + bw / 2, cy = ny + 1;
        if (i > 0 && prevon && sld[i - 1]) line_rgb(prevcx, prevcy, cx, cy, hex);  // glide from the previous note
        prevcx = cx; prevcy = cy; prevon = 1;
    }
}

// a drum step strip: lit cells in the machine colour, dim cells for the rests, a pip on accents
static void drumstrip(int x, int y, int w, int h, int hex, const int *on, const int *acc) {
    int N = 8;
    for (int i = 0; i < N; i++) {
        int bx = x + i * w / N, bw = (x + (i + 1) * w / N) - bx - 1;
        rectfill_rgb(bx, y + 2, bw, h - 4, on[i] ? hex : C_CELL);
        if (on[i] && acc[i]) rectfill_rgb(bx, y, bw, 2, C_INK);   // accent pip above
    }
}

void draw(void) {
    int W = screen_w(), H = screen_h();
    rectfill_rgb(0, 0, W, H, C_BG);

    // the four strips down the left; the CUT knob hero on the right
    int sx = 7, sw = 76, sh = 25, gap = 4, top = 7;
    static const int p_a[8] = { 1,1,3,2,4,2,5,3 },  s_a[8] = { 0,1,0,0,1,0,0,0 };
    static const int p_b[8] = { 4,4,2,5,3,5,2,4 },  s_b[8] = { 1,0,0,1,0,0,1,0 };
    static const int on8[8] = { 1,0,0,0,1,0,1,0 },  ac8[8] = { 1,0,0,0,0,0,0,0 };
    static const int on9[8] = { 0,1,0,1,0,1,0,1 },  ac9[8] = { 0,0,0,0,0,0,0,1 };
    ladder   (sx, top + 0 * (sh + gap), sw, sh, C_303A, p_a, s_a);
    ladder   (sx, top + 1 * (sh + gap), sw, sh, C_303B, p_b, s_b);
    drumstrip(sx, top + 2 * (sh + gap), sw, sh, C_808,  on8, ac8);
    drumstrip(sx, top + 3 * (sh + gap), sw, sh, C_909,  on9, ac9);

    // the hero CUT knob
    int kx = 105, ky = H / 2, kr = 19;
    disc_rgb(kx, ky, kr, C_CELL);          // dark body
    ring_rgb(kx, ky, kr, 3, C_303A);       // orange rim
    disc_rgb(kx, ky, 4, C_303A);           // hub
    float a = -0.85f;                      // pointer turned up-right (filter open)
    int px = kx + (int)(cosf(a) * (kr - 4)), py = ky + (int)(sinf(a) * (kr - 4));
    line_rgb(kx, ky, px, py, C_INK);
    line_rgb(kx + 1, ky, px + 1, py, C_INK);   // 2px pointer
}
