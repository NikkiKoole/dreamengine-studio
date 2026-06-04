#include "studio.h"

// PALETTE LAB — try on a new default palette without touching anything else,
// and judge it where it matters most: BLENDING. This is blendlab x palettes —
// the Layer-1/1b probe from docs/design/palette-and-color.md, with blendlab's
// scene-function architecture ported in so every blend runs against the LIVE
// candidate. Two probes, one verdict machine: a palette is only as good as
// what `nearest(mix(a,b))` can land on.
//
// Built on the EXPERIMENTAL palette_hex(i, 0xRRGGBB) and the 64-slot palette
// (slots 32-63 mirror 0-31 by default, so nothing else changed). Scenes draw
// with normal CLR_* indices and re-skin instantly on palette switch; the blend
// tables (AVG/ADD/MUL, blendlab's trio) are rebuilt from the candidate's RGB
// on every switch — candidates with more/denser colors visibly band less.
//
// Four candidates (keys 1-4):
//   1  PICO-8 (shipped)     — the baseline we're trying to replace
//   2  ENDESGA 32           — 32 role-mapped, upper half mirrors
//   3  RESURRECT 64 (full)  — 32 role-mapped + the other 32 in slots 32-63
//   4  E32 + 32 DERIVED     — the "blended in-betweens" idea: slots 32-63 are
//      sRGB midpoints of ramp neighbours + hue bridges, computed here
//      (OKLab mixing would be kinder — judging that muddiness is the point)
//
// NOTE the derived/upper colors only show where something SAMPLES them — the
// dither scenes (sunset/portrait) reference indices 0-31 and are identical for
// candidates 2 and 4 by construction. The blend scenes are where 32 vs 64
// separates; the D toggle puts today's fillp fake next to the real thing.
//
// Role-mapping notes: slot 8 must stay "the red", ramps must still ramp, or
// every existing cart scrambles. Weak fits are flagged in the tables (E32 has
// no lime, no second dark plum, no near-black navy pair — three dup slots);
// the Resurrect cut filled every role. That asymmetry is already a finding.
//
// CAVEAT while a custom palette is active: pal(c0,c1) would inject the SHIPPED
// c1 color (base_palette stays the texel-matching key) — scenes avoid pal().
//
// CONTROLS: 1-4 palette · LEFT/RIGHT scene · D dither-fake vs real blend ·
// C glass color · T cycle AVG/ADD/MUL in the table view · mouse = glow/pane.

// ---- the shipped palette, for the blend tables + completeness -----------
static const int PAL_PICO[32] = {
    0x000000, 0x1d2b53, 0x7e2553, 0x008751, 0xab5236, 0x5f574f, 0xc2c3c7, 0xfff1e8,
    0xff004d, 0xffa300, 0xffec27, 0x00e436, 0x29adff, 0x83769c, 0xff77a8, 0xffccaa,
    0x291814, 0x111d35, 0x422136, 0x125359, 0x742f29, 0x49333b, 0xa28879, 0xf3ef7d,
    0xbe1250, 0xff6c24, 0xa8e72e, 0x00b543, 0x065ab5, 0x754665, 0xff6e59, 0xff9d81,
};

// ENDESGA 32 (by ENDESGA, lospec.com/palette-list/endesga-32), pico-role order.
// Weak fits: 17 dups 1, 18 dups 16, 26 dups 11.
static const int PAL_E32[32] = {
    0x181425, 0x262b44, 0x68386c, 0x265c42, 0xbe4a2f, 0x3a4466, 0xc0cbdc, 0xffffff,
    0xff0044, 0xf77622, 0xfee761, 0x63c74d, 0x0099db, 0x8b9bb4, 0xf6757a, 0xe8b796,
    0x3e2731, 0x262b44, 0x3e2731, 0x193c3e, 0x733e39, 0x5a6988, 0xc28569, 0xead4aa,
    0xa22633, 0xd77643, 0x63c74d, 0x3e8948, 0x124e89, 0xb55088, 0xe43b44, 0xe4a672,
};

// RESURRECT 64 (by Kerrie Lake, lospec.com/palette-list/resurrect-64):
// 32 role-mapped to the pico slots + the remaining 32 in the upper half.
static const int PAL_R32[32] = {
    0x2e222f, 0x484a77, 0x6b3e75, 0x165a4c, 0x9e4539, 0x3e3546, 0x9babb2, 0xffffff,
    0xe83b3b, 0xfb6b1d, 0xf9c22b, 0x1ebc73, 0x4d9be6, 0x7f708a, 0xed8099, 0xfdcbb0,
    0x45293f, 0x323353, 0x753c54, 0x0b5e65, 0x6e2727, 0x625565, 0xab947a, 0xfbff86,
    0xae2334, 0xcd683d, 0xd5e04b, 0x239063, 0x4d65b4, 0xa24b6f, 0xf68181, 0xfca790,
};
static const int PAL_R64X[32] = {   // the other 32 of Resurrect 64
    0x966c6c, 0x694f62, 0xc7dcd0, 0xb33831, 0xea4f36, 0xf57d4a, 0xf79617, 0x7a3045,
    0xe6904e, 0xfbb954, 0x4c3e24, 0x676633, 0xa2a947, 0x91db69, 0xcddf6c, 0x313638,
    0x374e4a, 0x547e64, 0x92a984, 0xb2ba90, 0x0b8a8f, 0x0eaf9b, 0x30e1b9, 0x8ff8e2,
    0x8fd3ff, 0x905ea9, 0xa884f3, 0xeaaded, 0xcf657f, 0x831c5d, 0xc32454, 0xf04f78,
};

// candidate 4: E32 + derived in-betweens — midpoint pairs (ramp neighbours +
// hue bridges so cross-ramp blends have somewhere to land)
static const int MIX_PAIRS[32][2] = {
    {0,1},{1,12},{12,6},{6,7},        // sky
    {16,4},{4,9},{9,10},{10,23},      // warm
    {19,3},{3,27},{27,11},{11,26},    // green
    {0,21},{21,5},{5,22},{22,15},     // grey -> warm light
    {16,20},{20,30},{30,15},{15,31},  // skin
    {8,24},{24,2},{2,29},{29,14},     // reds/purples
    {1,28},{28,12},{13,5},{14,15},    // blues + misc
    {8,12},{9,12},{11,12},{10,7},     // hue bridges (glow-over-water class)
};

#define NPAL 4
static int cur_pal;        // 0 pico, 1 e32, 2 r64 full, 3 e32+derived
static int pal_n;          // how many DISTINCT colors the candidate brings (32 or 64)
static int cur_hex[64];    // active palette as hexes — feeds the blend tables
static int scene;
#define NSCENES 6
static int dither_fake;    // D — draw the blend shapes the way carts fake them today
static int glass_i;        // C — which glass color
static int table_i;        // T — which table the grid shows

static const int  GLASS_CLR[6] = { CLR_RED, CLR_TRUE_BLUE, CLR_YELLOW, CLR_MEDIUM_GREEN, CLR_WHITE, CLR_BLACK };
static const char *GLASS_NM[6] = { "red", "blue", "yellow", "green", "white", "black" };

// ---- blend tables, built from the LIVE candidate (blendlab's trio) ------
static unsigned char t_avg[64][64];   // (s+d)/2       — translucency / glass / shadow
static unsigned char t_add[64][64];   // min(s+d,255)  — additive glow / light
static unsigned char t_mul[64][64];   // s*d/255       — darken / fog / tint

static int mix_hex(int a, int b) {   // plain sRGB midpoint (OKLab would be kinder)
    return ((((a >> 16 & 255) + (b >> 16 & 255)) / 2) << 16) |
           ((((a >> 8  & 255) + (b >> 8  & 255)) / 2) << 8)  |
            (((a       & 255) + (b       & 255)) / 2);
}

static int nearest_idx(int r, int g, int b) {
    int best = 0; long bd = 0x7fffffff;
    for (int i = 0; i < pal_n; i++) {
        int dr = r - (cur_hex[i] >> 16 & 255), dg = g - (cur_hex[i] >> 8 & 255), db = b - (cur_hex[i] & 255);
        long d = 2L*dr*dr + 4L*dg*dg + 3L*db*db;   // rough luma weighting
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

static void build_tables(void) {
    for (int s = 0; s < pal_n; s++)
        for (int d = 0; d < pal_n; d++) {
            int sr = cur_hex[s] >> 16 & 255, sg = cur_hex[s] >> 8 & 255, sb = cur_hex[s] & 255;
            int dr = cur_hex[d] >> 16 & 255, dg = cur_hex[d] >> 8 & 255, db = cur_hex[d] & 255;
            t_avg[s][d] = (unsigned char)nearest_idx((sr+dr)/2, (sg+dg)/2, (sb+db)/2);
            t_add[s][d] = (unsigned char)nearest_idx(min(sr+dr,255), min(sg+dg,255), min(sb+db,255));
            t_mul[s][d] = (unsigned char)nearest_idx(sr*dr/255, sg*dg/255, sb*db/255);
        }
}

static void apply_palette(int which) {
    cur_pal = which;
    const int *lo = which == 0 ? PAL_PICO : which == 2 ? PAL_R32 : PAL_E32;
    for (int i = 0; i < 32; i++) cur_hex[i] = lo[i];
    if (which == 2)      { for (int i = 0; i < 32; i++) cur_hex[32 + i] = PAL_R64X[i]; pal_n = 64; }
    else if (which == 3) { for (int i = 0; i < 32; i++) cur_hex[32 + i] = mix_hex(PAL_E32[MIX_PAIRS[i][0]], PAL_E32[MIX_PAIRS[i][1]]); pal_n = 64; }
    else                 { for (int i = 0; i < 32; i++) cur_hex[32 + i] = lo[i]; pal_n = 32; }
    for (int i = 0; i < 64; i++) palette_hex(i, cur_hex[i]);
    build_tables();
}

// ---- scenes as pure (x,y) -> index functions (blendlab's architecture):
// one definition paints the screen AND answers "what's under this pixel?",
// so the blend reads dst with no pget feedback.
static int night_at(int x, int y) {
    if (x >= 36 && x < 148 && y >= 64 && y < 172) {            // gallery flat
        int wx = (x-36)/14, wy = (y-64)/18, ix = (x-36)%14, iy = (y-64)%18;
        if (ix >= 4 && ix < 11 && iy >= 5 && iy < 13) {
            int m = (wx*7 + wy*13) % 7;
            return (m < 2) ? CLR_LIGHT_YELLOW : (m == 2) ? CLR_ORANGE : CLR_DARKER_PURPLE;
        }
        return CLR_DARKER_GREY;
    }
    if (x >= 176 && x < 292 && y >= 88 && y < 172) {           // far tower, dimmer
        int wx = (x-176)/16, wy = (y-88)/16, ix = (x-176)%16, iy = (y-88)%16;
        if (ix >= 5 && ix < 12 && iy >= 4 && iy < 11)
            return ((wx*3 + wy*11) % 4 == 0) ? CLR_LIGHT_YELLOW : CLR_DARKER_BLUE;
        return CLR_BROWNISH_BLACK;
    }
    if (y >= 172) return CLR_BROWNISH_BLACK;                   // street
    if (y < 56 && ((x*31 + y*17) % 331) == 0) return CLR_LIGHT_GREY;  // stars
    if (y < 40) return CLR_BLACK;
    if (y < 90) return CLR_DARKER_BLUE;
    return CLR_DARK_BLUE;
}

static int day_at(int x, int y) {
    if (y >= 60 && y < 92) return (x/10) % 32;                 // all-32 swatch band
    { int ddx = x-70,  ddy = y-142; if (ddx*ddx + ddy*ddy <= 24*24) return CLR_MEDIUM_GREEN; }
    { int ddx = x-160, ddy = y-152; if (ddx*ddx + ddy*ddy <= 18*18) return CLR_ORANGE; }
    { int ddx = x-252, ddy = y-138; if (ddx*ddx + ddy*ddy <= 27*27) return CLR_TRUE_BLUE; }
    return (((x/16) + (y/16)) & 1) ? CLR_LIGHT_GREY : CLR_WHITE;   // tiled wall
}

static void draw_scene(int (*at)(int, int)) {
    for (int y = 0; y < SCREEN_H; y++) {
        int x = 0;
        while (x < SCREEN_W) {
            int c = at(x, y), x2 = x + 1;
            while (x2 < SCREEN_W && at(x2, y) == c) x2++;
            rectfill(x, y, x2 - x, 1, c);
            x = x2;
        }
    }
}

// ---- blended primitives (ported from blendlab, 64-wide tables) ----------
static void brect(int x, int y, int w, int h, int c, unsigned char tbl[64][64], int (*at)(int, int)) {
    for (int py = y; py < y + h; py++)
        for (int px = x; px < x + w; px++) {
            if (px < 0 || px >= SCREEN_W || py < 0 || py >= SCREEN_H) continue;
            pset(px, py, tbl[c][at(px, py)]);
        }
}
static void bcirc(int cx, int cy, int r, int c, unsigned char tbl[64][64], int (*at)(int, int)) {
    for (int yy = -r; yy <= r; yy++)
        for (int xx = -r; xx <= r; xx++) {
            if (xx*xx + yy*yy > r*r) continue;
            int px = cx + xx, py = cy + yy;
            if (px < 0 || px >= SCREEN_W || py < 0 || py >= SCREEN_H) continue;
            pset(px, py, tbl[c][at(px, py)]);
        }
}
static void glow(int cx, int cy, int r, int c_core, int c_mid, int c_rim, int (*at)(int, int)) {
    int r2 = r * r;
    for (int yy = -r; yy <= r; yy++)
        for (int xx = -r; xx <= r; xx++) {
            int d2 = xx*xx + yy*yy;
            if (d2 > r2) continue;
            int px = cx + xx, py = cy + yy;
            if (px < 0 || px >= SCREEN_W || py < 0 || py >= SCREEN_H) continue;
            int c = (d2 * 9 < r2) ? c_core : (d2 * 9 < r2 * 4) ? c_mid : c_rim;
            pset(px, py, t_add[c][at(px, py)]);
        }
}
static void glow_fake(int cx, int cy, int r, int c_core, int c_mid, int c_rim) {
    fillp(FILL_DOTS, -1);    circfill(cx, cy, r,     c_rim);
    fillp(FILL_CHECKER, -1); circfill(cx, cy, r*2/3, c_mid);
    fillp_reset();           circfill(cx, cy, r/3,   c_core);
}

// ---- scene 0: swatches + dithered ramps ---------------------------------
static void scene_swatches(void) {
    cls(CLR_BLACK);
    for (int i = 0; i < 64; i++) {
        int x = 8 + (i % 16) * 19, y = 20 + (i / 16) * 15;
        rectfill(x, y, 17, 12, i);
        if (i % 16 == 0) { font(FONT_TINY); print(str("%d", i), x - 7, y + 3, CLR_MEDIUM_GREY); font(FONT_NORMAL); }
    }
    static const int ramps[5][6] = {
        { 0, 1, 12, 6, 7, -1 }, { 16, 4, 9, 10, 23, -1 }, { 19, 3, 27, 11, 26, -1 },
        { 0, 21, 5, 22, 15, -1 }, { 16, 20, 30, 15, 31, -1 },
    };
    static const char *names[5] = { "SKY", "WARM", "GREEN", "GREY", "SKIN" };
    for (int r = 0; r < 5; r++) {
        int y = 88 + r * 20;
        font(FONT_SMALL); print(names[r], 8, y + 3, CLR_LIGHT_GREY); font(FONT_NORMAL);
        int x = 44;
        for (int i = 0; i < 5 && ramps[r][i + 1] >= 0; i++) { hgradient(x, y, 53, 12, ramps[r][i], ramps[r][i + 1]); x += 53; }
    }
}

// ---- scene 1: sunset — the dither-school corpus stress ------------------
static void scene_sunset(void) {
    vgradient(0, 0, SCREEN_W, 60, CLR_DARK_BLUE, CLR_DARK_PURPLE);
    vgradient(0, 60, SCREEN_W, 50, CLR_DARK_PURPLE, CLR_ORANGE);
    vgradient(0, 110, SCREEN_W, 20, CLR_ORANGE, CLR_YELLOW);
    fillp(FILL_DOTS, -1);    circfill(240, 112, 26, CLR_YELLOW);       fillp_reset();
    fillp(FILL_CHECKER, -1); circfill(240, 112, 18, CLR_LIGHT_YELLOW); fillp_reset();
    circfill(240, 112, 11, CLR_WHITE);
    vgradient(0, 130, SCREEN_W, 50, CLR_BLUE, CLR_DARK_BLUE);
    fillp(FILL_HLINES, -1); rectfill(0, 134, SCREEN_W, 30, CLR_LIGHT_YELLOW); fillp_reset();
    for (int x = 0; x < SCREEN_W; x += 4) {
        int h = 18 + (int)(noise(x * 0.02f) * 22);
        rectfill(x, 130 - h, 4, h, CLR_DARK_GREEN);
    }
    fillp(FILL_CHECKER, -1); rectfill(0, 104, SCREEN_W, 26, CLR_LIGHT_GREY); fillp_reset();
    vgradient(0, 180, SCREEN_W, 20, CLR_PEACH, CLR_BROWN);
}

// ---- scene 2: portrait — skin tones are where palettes get cruel --------
static void scene_portrait(void) {
    vgradient(0, 0, SCREEN_W, SCREEN_H, CLR_INDIGO, CLR_DARKER_PURPLE);
    int cx = 160, cy = 96;
    ovalfill(cx, cy + 78, 52, 36, CLR_TRUE_BLUE);
    rectfill(cx - 9, cy + 34, 18, 16, CLR_LIGHT_PEACH);
    ovalfill(cx, cy, 40, 46, CLR_LIGHT_PEACH);
    ovalfill(cx, cy - 28, 42, 24, CLR_BROWN);
    ovalfill(cx - 30, cy - 10, 12, 22, CLR_BROWN);
    ovalfill(cx + 30, cy - 10, 12, 22, CLR_BROWN);
    fillp(FILL_CHECKER, -1); ovalfill(cx - 14, cy + 18, 9, 6, CLR_DARK_PEACH); fillp_reset();
    fillp(FILL_CHECKER, -1); ovalfill(cx + 14, cy + 18, 9, 6, CLR_DARK_PEACH); fillp_reset();
    ovalfill(cx - 14, cy - 2, 6, 4, CLR_WHITE);  pset(cx - 13, cy - 2, CLR_BROWNISH_BLACK);
    ovalfill(cx + 14, cy - 2, 6, 4, CLR_WHITE);  pset(cx + 15, cy - 2, CLR_BROWNISH_BLACK);
    ovalfill(cx, cy + 26, 8, 3, CLR_DARK_PEACH);
    fillp(FILL_CHECKER, -1); ovalfill(cx + 18, cy + 8, 18, 30, CLR_PEACH); fillp_reset();
    static const int skin[6] = { 16, 20, 4, 30, 15, 31 };
    for (int i = 0; i < 6; i++) rectfill(8 + i * 16, 168, 14, 14, skin[i]);
    font(FONT_SMALL); print("SKIN FAMILY 16 20 4 30 15 31", 8, 184, CLR_LIGHT_GREY); font(FONT_NORMAL);
}

// ---- scene 3: night glow (blendlab) — D toggles fake vs real ------------
static void scene_night(void) {
    draw_scene(night_at);
    static const int lamp_x[3] = { 52, 162, 268 };
    for (int i = 0; i < 3; i++) {
        if (dither_fake) glow_fake(lamp_x[i], 146, 26, CLR_LIGHT_YELLOW, CLR_ORANGE, CLR_DARK_BROWN);
        else             glow(lamp_x[i], 146, 26, CLR_LIGHT_YELLOW, CLR_ORANGE, CLR_DARK_BROWN, night_at);
        rectfill(lamp_x[i] - 1, 148, 2, 24, CLR_DARK_GREY);
        circfill(lamp_x[i], 146, 2, CLR_WHITE);
    }
    int mx = mouse_x(), my = mouse_y();                       // a glow you carry
    if (mx > 2 && my > 20 && mx < SCREEN_W - 2 && my < SCREEN_H - 26) {
        if (dither_fake) glow_fake(mx, my, 30, CLR_YELLOW, CLR_DARK_ORANGE, CLR_DARK_BROWN);
        else             glow(mx, my, 30, CLR_YELLOW, CLR_DARK_ORANGE, CLR_DARK_BROWN, night_at);
    }
}

// ---- scene 4: glass + fog (blendlab) — D toggle, C glass color ----------
static void scene_glass(void) {
    draw_scene(day_at);
    int cx = (int)(now() * 14) % (SCREEN_W + 140) - 70;       // drifting cloud shadows (MUL)
    if (dither_fake) {
        fillp(FILL_CHECKER, -1);
        circfill(cx, 36, 24, CLR_DARK_GREY); circfill(cx + 30, 44, 18, CLR_DARK_GREY);
        fillp_reset();
    } else {
        bcirc(cx, 36, 24, CLR_MEDIUM_GREY, t_mul, day_at);
        bcirc(cx + 30, 44, 18, CLR_MEDIUM_GREY, t_mul, day_at);
    }
    if (dither_fake) { fillp(FILL_CHECKER, -1); rectfill(0, 150, SCREEN_W, 36, CLR_LIGHT_GREY); fillp_reset(); }
    else             brect(0, 150, SCREEN_W, 36, CLR_LIGHT_GREY, t_avg, day_at);   // fog band
    int mx = clamp(mouse_x(), 40, SCREEN_W - 40), my = clamp(mouse_y(), 50, SCREEN_H - 50);
    int gc = GLASS_CLR[glass_i];                              // the pane on your mouse
    if (dither_fake) { fillp(FILL_CHECKER, -1); rectfill(mx - 38, my - 28, 76, 56, gc); fillp_reset(); }
    else             brect(mx - 38, my - 28, 76, 56, gc, t_avg, day_at);
    rect(mx - 38, my - 28, 76, 56, gc);
    print(GLASS_NM[glass_i], mx - 34, my - 24, gc == CLR_BLACK ? CLR_WHITE : gc);
}

// ---- scene 5: the raw table — eyeball where the snap bands --------------
static void scene_table(void) {
    cls(CLR_BROWNISH_BLACK);
    unsigned char (*tbl)[64] = (table_i == 0) ? t_avg : (table_i == 1) ? t_add : t_mul;
    const char *nm = (table_i == 0) ? "AVG  (s+d)/2" : (table_i == 1) ? "ADD  min(s+d,255)" : "MUL  s*d/255";
    int cell = pal_n == 64 ? 2 : 4, ox = 96, oy = 40;
    print_centered(str("%s   %dx%d", nm, pal_n, pal_n), SCREEN_W / 2, 16, CLR_LIGHT_YELLOW);
    print("src", ox - 36, oy + 58, CLR_LIGHT_GREY);
    print("dst", ox + 140, oy - 16, CLR_LIGHT_GREY);
    for (int s = 0; s < pal_n; s++) rectfill(ox - 2 - cell, oy + s*cell, cell, cell, s);
    for (int d = 0; d < pal_n; d++) rectfill(ox + d*cell, oy - 2 - cell, cell, cell, d);
    for (int s = 0; s < pal_n; s++)
        for (int d = 0; d < pal_n; d++)
            rectfill(ox + d*cell, oy + s*cell, cell, cell, tbl[s][d]);
    font(FONT_SMALL);
    print_centered("cell = table[src][dst]   T cycles AVG/ADD/MUL", SCREEN_W / 2, oy + pal_n*cell + 6, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);
}

void init(void) {
    apply_palette(2);    // boot on full Resurrect 64...
    scene = 3;           // ...over the blended night — the verdict view
}

void update(void) {
    for (int i = 0; i < NPAL; i++) if (keyp('1' + i)) apply_palette(i);
    if (keyp(KEY_RIGHT)) scene = (scene + 1) % NSCENES;
    if (keyp(KEY_LEFT))  scene = (scene + NSCENES - 1) % NSCENES;
    if (keyp('D')) dither_fake = !dither_fake;
    if (keyp('C')) glass_i = (glass_i + 1) % 6;
    if (keyp('T')) table_i = (table_i + 1) % 3;
}

void draw(void) {
    switch (scene) {
        case 0: scene_swatches(); break;
        case 1: scene_sunset();   break;
        case 2: scene_portrait(); break;
        case 3: scene_night();    break;
        case 4: scene_glass();    break;
        case 5: scene_table();    break;
    }
    static const char *pnames[NPAL] = { "1 PICO-8 (shipped)", "2 ENDESGA 32", "3 RESURRECT 64", "4 E32+32 DERIVED" };
    static const char *snames[NSCENES] = { "SWATCHES+RAMPS", "SUNSET", "PORTRAIT", "NIGHT GLOW", "GLASS+FOG", "BLEND TABLE" };
    rectfill(0, 0, SCREEN_W, 11, CLR_BLACK);
    print(pnames[cur_pal], 4, 2, CLR_WHITE);
    print_right(str("%s%s  %d/%d", scene >= 3 && scene <= 4 ? (dither_fake ? "FAKE " : "REAL ") : "",
                    snames[scene], scene + 1, NSCENES), 316, 2, CLR_LIGHT_GREY);
    font(FONT_SMALL);
    rectfill(0, 193, SCREEN_W, 7, CLR_BLACK);
    print("1-4 palette  LEFT/RIGHT scene  D fake/real  C glass  T table", 4, 194, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);
}
