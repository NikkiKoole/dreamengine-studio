#include "studio.h"

// BLEND LAB — can 32 colors fake real translucency? (the STATUS item 18 experiment)
//
// A blend table is a 32x32 grid of palette indices: drawing color `src` over a
// pixel that is already `dst` resolves to table[src][dst]. The mixing happens
// ONCE in RGB when the table is built, then gets snapped back to the nearest
// palette color — at draw time it's a pure lookup, the screen never holds
// anything but the 32 indices. This is how VGA-era games did translucency
// (Doom's TRANMAP, Allegro's color maps); Picotron does it today.
//
// Zero engine API used beyond what ships — the cart builds its own tables and
// blends per-pixel against a scene function it owns.
//
//   1  NIGHT  — additive streetlight glows (the galerijflat "lights gap")
//   2  GLASS  — translucent panes (AVG) + cloud shadows (MUL) + a fog band
//   3  TABLE  — the raw 32x32 lookup, to eyeball banding (arrows switch table)
//   D  dither-fake — the same shapes the way carts fake them TODAY (fillp)
//   P  pget-dst — read dst from last frame's canvas instead of the scene fn
//      (deliberately broken: shows the feedback artifact a real engine
//       implementation must avoid by reading the in-progress frame)
//
// In GLASS mode: arrows cycle the pane color, mouse moves the pane.

// ---- the palette in RGB (copied from studio.h's hex comments) ----------------
static const unsigned char PAL[32][3] = {
    {0x00,0x00,0x00},{0x1d,0x2b,0x53},{0x7e,0x25,0x53},{0x00,0x87,0x51},
    {0xab,0x52,0x36},{0x5f,0x57,0x4f},{0xc2,0xc3,0xc7},{0xff,0xf1,0xe8},
    {0xff,0x00,0x4d},{0xff,0xa3,0x00},{0xff,0xec,0x27},{0x00,0xe4,0x36},
    {0x29,0xad,0xff},{0x83,0x76,0x9c},{0xff,0x77,0xa8},{0xff,0xcc,0xaa},
    {0x29,0x18,0x14},{0x11,0x1d,0x35},{0x42,0x21,0x36},{0x12,0x53,0x59},
    {0x74,0x2f,0x29},{0x49,0x33,0x3b},{0xa2,0x88,0x79},{0xf3,0xef,0x7d},
    {0xbe,0x12,0x50},{0xff,0x6c,0x24},{0xa8,0xe7,0x2e},{0x00,0xb5,0x43},
    {0x06,0x5a,0xb5},{0x75,0x46,0x65},{0xff,0x6e,0x59},{0xff,0x9d,0x81},
};

static unsigned char t_avg[32][32];   // (s+d)/2          — translucency / glass / shadow
static unsigned char t_add[32][32];   // min(s+d, 255)    — additive glow / light
static unsigned char t_mul[32][32];   // s*d/255          — darken / fog / tint

static int nearest_idx(int r, int g, int b) {
    int best = 0;
    long bd = 0x7fffffff;
    for (int i = 0; i < 32; i++) {
        int dr = r - PAL[i][0], dg = g - PAL[i][1], db = b - PAL[i][2];
        long d = 2L*dr*dr + 4L*dg*dg + 3L*db*db;   // rough luma weighting
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

static void build_tables(void) {
    for (int s = 0; s < 32; s++)
        for (int d = 0; d < 32; d++) {
            int sr = PAL[s][0], sg = PAL[s][1], sb = PAL[s][2];
            int dr = PAL[d][0], dg = PAL[d][1], db = PAL[d][2];
            t_avg[s][d] = nearest_idx((sr+dr)/2, (sg+dg)/2, (sb+db)/2);
            t_add[s][d] = nearest_idx(min(sr+dr,255), min(sg+dg,255), min(sb+db,255));
            t_mul[s][d] = nearest_idx(sr*dr/255, sg*dg/255, sb*db/255);
        }
}

// ---- lab state ----------------------------------------------------------------
static int  mode;          // 0 night, 1 glass, 2 table view
static bool dither_fake;   // D — draw the shapes the way carts fake them today
static bool use_pget;      // P — dst from last frame's canvas (shows feedback!)
static int  glass_i;       // which glass color
static int  table_i;       // which table the grid shows

static const int  glass_clr[6]  = { CLR_RED, CLR_TRUE_BLUE, CLR_YELLOW,
                                    CLR_MEDIUM_GREEN, CLR_WHITE, CLR_BLACK };
static const char *glass_nm[6]  = { "red", "blue", "yellow", "green", "white", "black" };

// ---- scenes — each is a pure (x,y) -> palette index function -------------------
// the SAME function paints the background (via run-length rows) and answers
// "what's under this pixel?" for the blend — one definition, no divergence.

static int night_at(int x, int y) {
    // tower block 1 — gallery flat, lit windows
    if (x >= 36 && x < 148 && y >= 64 && y < 172) {
        int wx = (x-36)/14, wy = (y-64)/18, ix = (x-36)%14, iy = (y-64)%18;
        if (ix >= 4 && ix < 11 && iy >= 5 && iy < 13) {
            int m = (wx*7 + wy*13) % 7;
            return (m < 2) ? CLR_LIGHT_YELLOW : (m == 2) ? CLR_ORANGE : CLR_DARKER_PURPLE;
        }
        return CLR_DARKER_GREY;
    }
    // tower block 2 — further away, dimmer
    if (x >= 176 && x < 292 && y >= 88 && y < 172) {
        int wx = (x-176)/16, wy = (y-88)/16, ix = (x-176)%16, iy = (y-88)%16;
        if (ix >= 5 && ix < 12 && iy >= 4 && iy < 11)
            return ((wx*3 + wy*11) % 4 == 0) ? CLR_LIGHT_YELLOW : CLR_DARKER_BLUE;
        return CLR_BROWNISH_BLACK;
    }
    if (y >= 172) return CLR_BROWNISH_BLACK;                    // street
    if (y < 56 && ((x*31 + y*17) % 331) == 0) return CLR_LIGHT_GREY;  // stars
    if (y < 40) return CLR_BLACK;                               // high sky
    if (y < 90) return CLR_DARKER_BLUE;
    return CLR_DARK_BLUE;
}

static int day_at(int x, int y) {
    if (y >= 60 && y < 92) return (x/10) % 32;     // all-32 swatch band — the banding sweep
    // some props for the glass to reveal
    {
        int ddx = x-70,  ddy = y-142; if (ddx*ddx + ddy*ddy <= 24*24) return CLR_MEDIUM_GREEN;
    }
    {
        int ddx = x-160, ddy = y-152; if (ddx*ddx + ddy*ddy <= 18*18) return CLR_ORANGE;
    }
    {
        int ddx = x-252, ddy = y-138; if (ddx*ddx + ddy*ddy <= 27*27) return CLR_TRUE_BLUE;
    }
    return (((x/16) + (y/16)) & 1) ? CLR_LIGHT_GREY : CLR_WHITE;   // bright tiled wall
}

// paint a scene with run-length rows so the screen IS the function
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

// ---- blended primitives — the whole experiment is these few lines --------------

static int dst_at(int x, int y, int (*at)(int, int)) {
    return use_pget ? pget(x, y) : at(x, y);
}

static void brect(int x, int y, int w, int h, int c,
                  unsigned char tbl[32][32], int (*at)(int, int)) {
    for (int py = y; py < y + h; py++)
        for (int px = x; px < x + w; px++) {
            if (px < 0 || px >= SCREEN_W || py < 0 || py >= SCREEN_H) continue;
            pset(px, py, tbl[c][dst_at(px, py, at)]);
        }
}

static void bcirc(int cx, int cy, int r, int c,
                  unsigned char tbl[32][32], int (*at)(int, int)) {
    for (int yy = -r; yy <= r; yy++)
        for (int xx = -r; xx <= r; xx++) {
            if (xx*xx + yy*yy > r*r) continue;
            int px = cx + xx, py = cy + yy;
            if (px < 0 || px >= SCREEN_W || py < 0 || py >= SCREEN_H) continue;
            pset(px, py, tbl[c][dst_at(px, py, at)]);
        }
}

// additive glow with a 3-band falloff: bright core, warm mid, dim rim
static void glow(int cx, int cy, int r, int c_core, int c_mid, int c_rim,
                 int (*at)(int, int)) {
    int r2 = r * r;
    for (int yy = -r; yy <= r; yy++)
        for (int xx = -r; xx <= r; xx++) {
            int d2 = xx*xx + yy*yy;
            if (d2 > r2) continue;
            int px = cx + xx, py = cy + yy;
            if (px < 0 || px >= SCREEN_W || py < 0 || py >= SCREEN_H) continue;
            int c = (d2 * 9 < r2) ? c_core : (d2 * 9 < r2 * 4) ? c_mid : c_rim;
            pset(px, py, t_add[c][dst_at(px, py, at)]);
        }
}

// the same glow the way carts fake it TODAY — fillp dither rings
static void glow_fake(int cx, int cy, int r, int c_core, int c_mid, int c_rim) {
    fillp(FILL_DOTS, -1);    circfill(cx, cy, r,       c_rim);
    fillp(FILL_CHECKER, -1); circfill(cx, cy, r*2/3,   c_mid);
    fillp_reset();           circfill(cx, cy, r/3,     c_core);
}

// ---- modes ---------------------------------------------------------------------

static void draw_night(void) {
    draw_scene(night_at);
    // three streetlights along the road
    static const int lamp_x[3] = { 52, 162, 268 };
    for (int i = 0; i < 3; i++) {
        if (dither_fake) glow_fake(lamp_x[i], 146, 26, CLR_LIGHT_YELLOW, CLR_ORANGE, CLR_DARK_BROWN);
        else             glow(lamp_x[i], 146, 26, CLR_LIGHT_YELLOW, CLR_ORANGE, CLR_DARK_BROWN, night_at);
        rectfill(lamp_x[i] - 1, 148, 2, 24, CLR_DARK_GREY);     // pole, on top of its glow
        circfill(lamp_x[i], 146, 2, CLR_WHITE);                 // the bulb
    }
    // a glow you carry — move the mouse
    int mx = mouse_x(), my = mouse_y();
    if (mx > 2 && my > 20 && mx < SCREEN_W - 2 && my < SCREEN_H - 26) {
        if (dither_fake) glow_fake(mx, my, 30, CLR_YELLOW, CLR_DARK_ORANGE, CLR_DARK_BROWN);
        else             glow(mx, my, 30, CLR_YELLOW, CLR_DARK_ORANGE, CLR_DARK_BROWN, night_at);
    }
}

static void draw_glass(void) {
    draw_scene(day_at);
    // drifting cloud shadows — multiply darkens what's under them
    int cx = (int)(now() * 14) % (SCREEN_W + 140) - 70;
    if (dither_fake) {
        fillp(FILL_CHECKER, -1);
        circfill(cx, 36, 24, CLR_DARK_GREY); circfill(cx + 30, 44, 18, CLR_DARK_GREY);
        fillp_reset();
    } else {
        bcirc(cx, 36, 24, CLR_MEDIUM_GREY, t_mul, day_at);
        bcirc(cx + 30, 44, 18, CLR_MEDIUM_GREY, t_mul, day_at);
    }
    // a fog band rolling over the props
    if (dither_fake) {
        fillp(FILL_CHECKER, -1); rectfill(0, 150, SCREEN_W, 36, CLR_LIGHT_GREY); fillp_reset();
    } else {
        brect(0, 150, SCREEN_W, 36, CLR_LIGHT_GREY, t_avg, day_at);
    }
    // the pane of glass on your mouse
    int mx = clamp(mouse_x(), 40, SCREEN_W - 40), my = clamp(mouse_y(), 50, SCREEN_H - 50);
    int gc = glass_clr[glass_i];
    if (dither_fake) {
        fillp(FILL_CHECKER, -1); rectfill(mx - 38, my - 28, 76, 56, gc); fillp_reset();
    } else {
        brect(mx - 38, my - 28, 76, 56, gc, t_avg, day_at);
    }
    rect(mx - 38, my - 28, 76, 56, gc);
    print(glass_nm[glass_i], mx - 34, my - 24, gc == CLR_BLACK ? CLR_WHITE : gc);
}

static void draw_table(void) {
    cls(CLR_BROWNISH_BLACK);
    unsigned char (*tbl)[32] = (table_i == 0) ? t_avg : (table_i == 1) ? t_add : t_mul;
    const char *nm = (table_i == 0) ? "AVG  (s+d)/2" : (table_i == 1) ? "ADD  min(s+d,255)" : "MUL  s*d/255";
    int ox = 96, oy = 44;
    print_centered(nm, SCREEN_W / 2, 16, CLR_LIGHT_YELLOW);
    print("src", ox - 36, oy + 60, CLR_LIGHT_GREY);
    print("dst", ox + 144, oy - 18, CLR_LIGHT_GREY);
    for (int s = 0; s < 32; s++) rectfill(ox - 8, oy + s*4, 4, 4, s);       // src axis
    for (int d = 0; d < 32; d++) rectfill(ox + d*4, oy - 8, 4, 4, d);       // dst axis
    for (int s = 0; s < 32; s++)
        for (int d = 0; d < 32; d++)
            rectfill(ox + d*4, oy + s*4, 4, 4, tbl[s][d]);
    print_centered("cell = table[src][dst]", SCREEN_W / 2, oy + 134, CLR_MEDIUM_GREY);
}

// ---- loop ----------------------------------------------------------------------

void init(void) {
    build_tables();
}

void update(void) {
    if (keyp('1')) { mode = 0; hit(62, INSTR_TRI, 3, 50); }
    if (keyp('2')) { mode = 1; hit(66, INSTR_TRI, 3, 50); }
    if (keyp('3')) { mode = 2; hit(69, INSTR_TRI, 3, 50); }
    if (keyp('D')) { dither_fake = !dither_fake; hit(dither_fake ? 55 : 72, INSTR_SQUARE, 3, 40); }
    if (keyp('P')) { use_pget = !use_pget; hit(use_pget ? 50 : 74, INSTR_SQUARE, 3, 40); }
    if (mode == 1) {
        if (keyp(KEY_LEFT))  glass_i = (glass_i + 5) % 6;
        if (keyp(KEY_RIGHT)) glass_i = (glass_i + 1) % 6;
    }
    if (mode == 2) {
        if (keyp(KEY_LEFT))  table_i = (table_i + 2) % 3;
        if (keyp(KEY_RIGHT)) table_i = (table_i + 1) % 3;
    }
}

void draw(void) {
    if (mode == 0) draw_night();
    else if (mode == 1) draw_glass();
    else draw_table();

    // HUD
    rectfill(0, 0, SCREEN_W, 12, CLR_BLACK);
    print(mode == 0 ? "BLEND LAB  1:night ADD" :
          mode == 1 ? "BLEND LAB  2:glass AVG+MUL" :
                      "BLEND LAB  3:the tables", 4, 2, CLR_WHITE);
    if (dither_fake) print_right("DITHER FAKE", SCREEN_W - 4, 2, CLR_ORANGE);
    else if (use_pget) print_right("PGET DST (feedback!)", SCREEN_W - 4, 2, CLR_RED);
    else print_right("blend table", SCREEN_W - 4, 2, CLR_GREEN);

    rectfill(0, SCREEN_H - 12, SCREEN_W, 12, CLR_BLACK);
    print(mode == 1 ? "1/2/3 mode  D dither  P pget  arrows: glass color  mouse: pane" :
          mode == 2 ? "1/2/3 mode  arrows: switch table" :
                      "1/2/3 mode  D dither-fake  P pget-dst  mouse: carry a light",
          4, SCREEN_H - 10, CLR_LIGHT_GREY);
}
