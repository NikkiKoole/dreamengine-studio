/* de:meta
title: Tiny Pedalboard — App Icon
slug: pedalicon
kind: [toy]
teaches: [layout]
created: 2026-07-22
description:
  summary: The Tiny Pedalboard app icon — a full-bleed mosaic of the real effect tiles.
  detail: >
    Renders the App Store icon for Tiny Pedalboard straight from the app's own material:
    a square, edge-to-edge grid of the effect tiles (BITCRUSH / DELAY / REVERB / …), each
    the real pixel glyph on its body+accent colour, NO labels. Honestly pixel-made, not an
    AI illustration. The FxDef table + the four macro glyphs (lofi/fuzz/shimmer/od) are
    copied verbatim from pedalboard.c so the tiles match the app byte-for-byte; the shared
    fx_icon() draws the rest. Render at a small square canvas, then integer-upscale to 1024.
      node tools/play.js pedalicon run --headless --screen 256x256 --frames 1 --dump build/.icon
      ffmpeg -y -i build/.icon/frame_00000.png -vf scale=1024:1024:flags=neighbor icon-1024.png
  controls: none — a static render.
*/
#include "studio.h"
#include "fxicons.h"     // shared effect glyphs (fx_icon) + the canonical effect palette
#include <math.h>

// ── the effect catalogue — copied VERBATIM from tools/carts/pedalboard.c so the icon tiles
//    are byte-identical to the app's. Keep in sync if pedalboard's CAT[] changes (rare). ──
#define MAXK 4
typedef struct {
    const char *name; int body, accent, kind, nk;
    const char *klabel[MAXK]; float kdef[MAXK];
} FxDef;
enum { C_BIT, C_EQ, C_CHO, C_PHA, C_FLG, C_TAP, C_TRM, C_WAH, C_RVB, C_FMT, C_PAN, C_FIL, C_RNG, C_DLY, C_LOFI, C_FUZZ, C_GRN, C_EQ2, C_OD, C_SHW, C_GATE, C_SHMR, NCAT };
static const FxDef CAT[NCAT] = {
    { "BITCRUSH", CLR_DARK_BROWN,    CLR_DARK_ORANGE,  FX_CRUSH,   3, { "BIT","RTE","MIX" },   { 0.50f, 0.40f, 0.60f } },
    { "EQ",       CLR_DARKER_BLUE,   CLR_BLUE,         FX_EQ,      3, { "LO","MID","HI" },     { 0.50f, 0.50f, 0.50f } },
    { "CHORUS",   CLR_DARKER_PURPLE, CLR_PINK,         FX_CHORUS,  3, { "RTE","DEP","MIX" },   { 0.30f, 0.28f, 0.45f } },
    { "PHASER",   CLR_DARK_GREEN,    CLR_LIME_GREEN,   FX_PHASER,  4, { "RTE","DEP","FB","MX" },{ 0.30f, 0.70f, 0.65f, 0.55f } },
    { "FLANGER",  CLR_BLUE_GREEN,    CLR_MEDIUM_GREEN, FX_FLANGER, 4, { "RT","DP","FB","MX" }, { 0.20f, 0.70f, 0.60f, 0.50f } },
    { "TAPE",     CLR_DARK_RED,      CLR_PEACH,        FX_TAPE,    3, { "WOW","FLT","SAT" },   { 0.35f, 0.25f, 0.45f } },
    { "TREMOLO",  CLR_DARKER_GREY,   CLR_LIGHT_YELLOW, FX_TREM,    3, { "SPD","DEP","WAV" },   { 0.45f, 0.60f, 0.0f } },
    { "WAH",      CLR_DARK_PURPLE,   CLR_MAUVE,        FX_WAH,     4, { "SNS","RES","MIX","MOD" },{ 0.50f, 0.55f, 0.70f, 0.0f } },
    { "REVERB",   CLR_DARK_BLUE,     CLR_INDIGO,       FX_REVERB,  3, { "SIZ","DMP","MIX" },   { 0.70f, 0.40f, 0.45f } },
    { "VOWEL",    CLR_BROWN,         CLR_LIGHT_PEACH,  FX_FORMANT, 4, { "VWL","Q","MIX","MOD" },{ 0.50f, 0.60f, 0.90f, 0.0f } },
    { "AUTOPAN",  CLR_DARK_GREY,     CLR_LIGHT_YELLOW, FX_PAN,     3, { "SPD","DEP","WAV" },   { 0.35f, 0.70f, 0.0f } },
    { "FILTER",   CLR_TRUE_BLUE,     CLR_BLUE,         FX_FILTER,  3, { "CUT","RES","MOD" },   { 0.50f, 0.30f, 0.0f } },
    { "RINGMOD",  CLR_INDIGO,        CLR_GREEN,        FX_RINGMOD, 2, { "FRQ","MIX" },         { 0.30f, 0.80f } },
    { "DELAY",    CLR_DARK_PEACH,    CLR_ORANGE,       FX_ECHO,    4, { "TIM","FB","TON","MIX" },{ 0.35f, 0.40f, 0.55f, 0.45f } },
    { "LO-FI",    CLR_DARK_BROWN,    CLR_PEACH,        -1,         3, { "AMT","WOW","TON" },   { 0.50f, 0.40f, 0.45f } },
    { "FUZZ",     CLR_DARK_RED,      CLR_ORANGE,       -2,         2, { "FUZZ","MODE" },       { 0.65f, 0.0f } },
    { "GRAINS",   CLR_INDIGO,        CLR_MAUVE,        FX_GRAINS,  4, { "SIZE","DENS","MIX","FRZ" }, { 0.30f, 0.30f, 0.50f, 0.0f } },
    { "EQ2",      CLR_DARKER_BLUE,   CLR_TRUE_BLUE,    FX_EQ,      3, { "LO","MID","HI" },     { 0.50f, 0.50f, 0.50f } },
    { "OD",       CLR_DARK_ORANGE,   CLR_PEACH,        FX_DRIVE,   4, { "DRV","VOICE","TONE","MIX" },  { 0.50f, 0.0f, 0.5f, 1.00f } },
    { "SHALLOW",  CLR_DARKER_BLUE,   CLR_BLUE,         FX_SHALLOW, 3, { "RATE","DEP","MIX" },  { 0.30f, 0.60f, 0.50f } },
    { "GATE",     CLR_DARK_GREEN,    CLR_GREEN,        FX_GATE,    3, { "THR","ATK","REL" },   { 0.35f, 0.10f, 0.40f } },
    { "SHIMMER",  CLR_DARKER_BLUE,   CLR_INDIGO,       -3,         4, { "SIZE","DMP","SHM","MIX" }, { 0.85f, 0.40f, 0.60f, 0.45f } },
};

// the four MACRO-pedal glyphs (no single FX_* kind) — verbatim from pedalboard.c
static void lofi_icon(int cx, int cy, int col) {
    rrect(cx - 12, cy - 7, 24, 14, 2, col);
    circ(cx - 5, cy - 2, 2, col); circ(cx + 5, cy - 2, 2, col);
    line(cx - 3, cy - 2, cx + 3, cy - 2, col);
    line(cx - 6, cy + 4, cx + 6, cy + 4, col);
}
static void fuzz_icon(int cx, int cy, int col) {
    for (int a = 0; a < 8; a++) {
        float ang = a * 0.7853982f; int r = (a & 1) ? 4 : 8;
        line(cx, cy, cx + (int)(cosf(ang) * r), cy + (int)(sinf(ang) * r), col);
    }
    circfill(cx, cy, 2, col);
}
static void shimmer_icon(int cx, int cy, int col) {
    for (int i = 0; i < 3; i++) {
        int y = cy + 6 - i * 5;
        line(cx - 7, y, cx, y - 5, col); line(cx, y - 5, cx + 7, y, col);
    }
    pset(cx, cy - 9, col); circfill(cx + 5, cy - 8, 1, col);
}
static void od_icon(int cx, int cy, int col) {
    line(cx - 12, cy + 4, cx - 8, cy - 4, col); line(cx - 8, cy - 4, cx - 2, cy - 4, col);
    line(cx - 2, cy - 4, cx + 2, cy + 4, col);  line(cx + 2, cy + 4, cx + 8, cy + 4, col);
    line(cx + 8, cy + 4, cx + 12, cy - 4, col);
}

// one tile: colored rounded body + accent border + the centred glyph (NO label)
static void tile(int cat, int x, int y, int w, int h) {
    const FxDef *d = &CAT[cat];
    rrectfill(x, y, w, h, 3, d->body);
    rrect(x, y, w, h, 3, d->accent);
    int cx = x + w / 2, cy = y + h / 2;
    if      (d->kind == -1)       lofi_icon(cx, cy, d->accent);
    else if (d->kind == -2)       fuzz_icon(cx, cy, d->accent);
    else if (d->kind == -3)       shimmer_icon(cx, cy, d->accent);
    else if (d->kind == FX_DRIVE) od_icon(cx, cy, d->accent);
    else                          fx_icon(d->kind, cx, cy, d->accent, d->body);
}

// full-bleed layout: a 5x4 grid of 20 tiles, then a DARK bottom row — one tile anchoring each
// corner (green GATE left, orange DELAY right) with black open space across the middle. 22 tiles,
// no repeats. Edge tiles run to the canvas edge (no dead margin); dark gutters sit BETWEEN tiles,
// and the black bottom middle is the SAME dark, so it reads as intentional negative space.
#define CORNER_L C_GATE
#define CORNER_R C_DLY
void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    int W = screen_w(), H = screen_h();
    int rows = 5, cols = 5, g = 2;

    // the 20 grid tiles = every effect except the two corner anchors, in catalogue order
    int grid[20], gi = 0;
    for (int i = 0; i < NCAT; i++) if (i != CORNER_L && i != CORNER_R) grid[gi++] = i;

    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < cols; c++) {
            int x0 = c * W / cols, x1 = (c + 1) * W / cols;
            int y0 = r * H / rows, y1 = (r + 1) * H / rows;
            int x = x0, y = y0, w = x1 - x0, h = y1 - y0;
            if (c < cols - 1) w -= g;
            h -= g;                                // gutter below
            tile(grid[r * cols + c], x, y, w, h);
        }
    }
    // dark bottom row: a tile at each side, black open space in the middle
    int y0 = 4 * H / rows;
    tile(CORNER_L, 0,             y0, W / cols - g, H - y0);   // left corner
    tile(CORNER_R, 4 * W / cols,  y0, W - 4 * W / cols, H - y0); // right corner
}
