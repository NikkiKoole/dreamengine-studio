/* de:meta
{
  "title": "Deluxe Paint",
  "status": "active",
  "created": "2026-05-31",
  "kind": [
    "tool"
  ],
  "teaches": [
    "dithering-gradient",
    "palette-cycling"
  ],
  "lineage": "Direct homage to Amiga Deluxe Paint; adds Bayer-matrix gradient tool and live pal()-driven color cycling as the showpiece features beyond the standard paint toolkit.",
  "homage": "Deluxe Paint (1985)",
  "description": "A full pixel-paint program - pencil/line/rect/oval/fill/airbrush/eyedropper/custom-brush/mirror/undo, plus dither PATTERN fills, a Bayer GRADIENT tool, and live COLOR CYCLING (fire/water/rainbow ramps that flow via pal()). Mouse: left=FG, right=BG, click toolbar/palette/patterns, drag for shapes. Keys: b/l/r/o/f/s/i/c/d tools, [ ] size, x swap, m mirror, h hole, u undo."
}
de:meta */
#include "studio.h"

// ── DELUXE PAINT ──────────────────────────────────────────────────────────
// A pixel-paint program in a cartridge — a love letter to Amiga DPaint, built
// on mouse + pset + the 32-color palette. It does the bread-and-butter tools
// (pencil, line, rect, oval, flood fill, airbrush, eyedropper, custom brush,
// mirror, undo) AND three things the engine does better than a tile editor:
//
//   • PATTERN fills  — every tool lays a 4×4 dither texture (the fillp masks),
//                      two-tone, with an optional transparent "hole" color.
//   • DITHERED GRADIENT — drag a box, get a fg→bg Bayer-dithered ramp.
//   • COLOR CYCLING  — pick a ramp and animate it: because the canvas is just
//                      palette indices replayed through pal() each frame, the
//                      pixels in that ramp flow live (water / fire). Showpiece.
//
// The canvas is an int grid we replay with pset every frame; the live tool
// preview draws on top. Mouse is the whole experience; keys are shortcuts.
//
//   LEFT  paint with the current tool + FG     RIGHT  paint with BG (secondary)
//   click toolbar / palette / patterns to pick · drag for line/rect/oval/grad
//   keys: b pencil  l line  r rect  o oval  f fill  s spray  i pick
//         c custom-brush  d gradient  p poly  z bezier · [ ] brush size · x swap · m mirror
//         h hole · u undo · esc cancel poly/bezier
//
// fillp polarity (from studio.c): bit (pat>>(15-i))&1 — 1-bits take the HOLE
// color, 0-bits take the draw color. So a fully SOLID fill is 0x0000, and
// 0xFFFF is "all holes". We follow that exactly in pat_at().

// ── canvas + layout ───────────────────────────────────────────────────────
#define CW 192
#define CH 144
#define CANVAS_X 44
#define CANVAS_Y 14

#define TBX 2
#define TBY 14
#define TBW 17
#define TBH 17

#define SBX 242            // sidebar left edge

enum { T_PENCIL, T_LINE, T_RECT, T_OVAL, T_FILL, T_SPRAY, T_PICK, T_BRUSH, T_GRAD, T_POLY, T_BEZIER, T_COUNT };
static const char *TOOLNAME[T_COUNT] = {
    "PENCIL","LINE","RECT","OVAL","FILL","SPRAY","PICK","BRUSH","GRADIENT","POLY","BEZIER"
};

// ── state ───────────────────────────────────────────────────────────────────
static unsigned char canvas[CH][CW];
static int  fg = CLR_WHITE, bg = CLR_BLACK;
static int  tool = T_PENCIL;
static int  brush = 1;                 // brush radius+1 (1 = single pixel)
static int  pat = 0x0000;              // current fill pattern (0x0000 = solid)
static int  patHole = -1;              // 1-bit color (-1 = transparent)
static bool filled = true;             // rect/oval fill mode
static bool mirx = false, miry = false;

// color cycling
static bool cycleOn = true;
static int  rampSel = 1;               // which preset ramp
static float cycSpeed = 5.0f;
static float cycPhase = 0.0f;
#define RAMPS 3
static const int  RAMP_N[RAMPS]   = { 6, 6, 7 };
static const int  RAMP_IDX[RAMPS][8] = {
    { CLR_DARK_RED, CLR_RED, CLR_DARK_ORANGE, CLR_ORANGE, CLR_YELLOW, CLR_LIGHT_YELLOW },        // FIRE
    { CLR_DARK_BLUE, CLR_DARKER_BLUE, CLR_TRUE_BLUE, CLR_BLUE, CLR_BLUE_GREEN, CLR_LIGHT_GREY }, // WATER
    { CLR_RED, CLR_ORANGE, CLR_YELLOW, CLR_GREEN, CLR_BLUE, CLR_INDIGO, CLR_PINK },              // RAINBOW
};
static const char *RAMP_NAME[RAMPS] = { "FIRE", "WATER", "RAIN" };

// fill patterns offered in the picker (our polarity: 0-bit=draw, 1-bit=hole)
#define NPAT 10
static const int PATS[NPAT] = {
    0x0000, // solid
    0x8020, // sparse dots
    0xA5A5, // 50% checker
    0x5A5A, // checker alt
    0xFAFA, // dense
    0xF0F0, // h-lines
    0xAAAA, // v-lines
    0x8421, // diagonal
    0xF888, // grid / brick
    0xFFFF, // all holes (stencil)
};

// drag / stroke bookkeeping
static bool dragging = false;
static int  dsx, dsy;
static bool dragRight = false;
static bool painting = false;
static int  lastx, lasty;
static bool grabMode = false;

// bezier: after dragging start→end, phase2 lets the mouse set the control point
static int  bez_x0, bez_y0, bez_x1, bez_y1;
static bool bez_phase2 = false;
static bool bez_right  = false;

// in-progress polygon vertices
#define POLY_MAX 32
static int  poly_pts[POLY_MAX * 2];
static int  poly_n = 0;

// custom brush
#define CBR 56
static unsigned char cbr[CBR][CBR];
static int  cbw = 0, cbh = 0;
static bool hasbr = false;

// undo ring
#define UNDO_N 6
static unsigned char undostack[UNDO_N][CH][CW];
static int undotop = 0, undocount = 0;

// flood-fill scratch
static unsigned char visited[CH][CW];
static int fstack[CW * CH + 64];

// per-frame mouse + cursor cache (for UI in draw)
static int  M_X, M_Y;
static int  curcx, curcy;
static bool curin;
static float msgT = 0; static const char *msg = "";

// 4×4 Bayer matrix for the dithered gradient
static const int BAYER[4][4] = {
    {  0,  8,  2, 10 },
    { 12,  4, 14,  6 },
    {  3, 11,  1,  9 },
    { 15,  7, 13,  5 },
};

// ── canvas writers (all honor bounds, mirror, and the fill pattern) ─────────
static void cset(int x, int y, int col) {
    if (col < 0) return;
    if (x >= 0 && x < CW && y >= 0 && y < CH) canvas[y][x] = (unsigned char)col;
    if (mirx) { int mx = CW - 1 - x; if (mx >= 0 && mx < CW && y >= 0 && y < CH) canvas[y][mx] = (unsigned char)col; }
    if (miry) { int my = CH - 1 - y; if (x >= 0 && x < CW && my >= 0 && my < CH) canvas[my][x] = (unsigned char)col; }
    if (mirx && miry) { int mx = CW - 1 - x, my = CH - 1 - y;
        if (mx >= 0 && mx < CW && my >= 0 && my < CH) canvas[my][mx] = (unsigned char)col; }
}

// color to lay at (x,y) for draw color `draw`, through the active pattern
static int pat_at(int x, int y, int draw) {
    int i  = ((y & 3) * 4) + (x & 3);
    int on = (pat >> (15 - i)) & 1;
    return on ? patHole : draw;      // 1-bit -> hole (-1 skips), 0-bit -> draw
}

static void cdab(int cx, int cy, int draw) {
    int r = brush - 1;
    for (int dy = -r; dy <= r; dy++)
        for (int dx = -r; dx <= r; dx++) {
            if (dx * dx + dy * dy > r * r + 1) continue;   // round-ish brush
            int x = cx + dx, y = cy + dy;
            cset(x, y, pat_at(x, y, draw));
        }
}

static void cline(int x0, int y0, int x1, int y1, int draw) {
    int dx = abs(x1 - x0), dy = -abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy;
    while (1) {
        cdab(x0, y0, draw);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static void crect(int x0, int y0, int x1, int y1, int draw, bool fill) {
    int xa = min(x0, x1), xb = max(x0, x1), ya = min(y0, y1), yb = max(y0, y1);
    if (fill) {
        for (int y = ya; y <= yb; y++)
            for (int x = xa; x <= xb; x++) cset(x, y, pat_at(x, y, draw));
    } else {
        for (int x = xa; x <= xb; x++) { cdab(x, ya, draw); cdab(x, yb, draw); }
        for (int y = ya; y <= yb; y++) { cdab(xa, y, draw); cdab(xb, y, draw); }
    }
}

static void coval(int x0, int y0, int x1, int y1, int draw, bool fill) {
    int xa = min(x0, x1), xb = max(x0, x1), ya = min(y0, y1), yb = max(y0, y1);
    float rx = (xb - xa) / 2.0f, ry = (yb - ya) / 2.0f;
    float ox = (xa + xb) / 2.0f, oy = (ya + yb) / 2.0f;
    if (rx < 0.5f || ry < 0.5f) { cline(xa, ya, xb, yb, draw); return; }
    for (int y = ya; y <= yb; y++)
        for (int x = xa; x <= xb; x++) {
            float nx = (x - ox) / rx, ny = (y - oy) / ry;
            float d = nx * nx + ny * ny;
            if (d > 1.0f) continue;
            if (fill) { cset(x, y, pat_at(x, y, draw)); }
            else {
                float l = (x - 1 - ox) / rx, rr = (x + 1 - ox) / rx;
                float u = (y - 1 - oy) / ry, b = (y + 1 - oy) / ry;
                bool edge = l * l + ny * ny > 1.0f || rr * rr + ny * ny > 1.0f ||
                            nx * nx + u * u > 1.0f || nx * nx + b * b > 1.0f;
                if (edge) cdab(x, y, draw);
            }
        }
}

static void cflood(int sx, int sy, int draw) {
    if (sx < 0 || sy < 0 || sx >= CW || sy >= CH) return;
    int target = canvas[sy][sx];
    for (int y = 0; y < CH; y++) for (int x = 0; x < CW; x++) visited[y][x] = 0;
    int top = 0;
    visited[sy][sx] = 1; fstack[top++] = sy * CW + sx;
    while (top > 0) {
        int p = fstack[--top], x = p % CW, y = p / CW;
        int col = pat_at(x, y, draw);
        if (col >= 0) canvas[y][x] = (unsigned char)col;
        int nx, ny;
        nx = x + 1; ny = y; if (nx < CW  && !visited[ny][nx] && canvas[ny][nx] == target) { visited[ny][nx] = 1; fstack[top++] = ny * CW + nx; }
        nx = x - 1; ny = y; if (nx >= 0  && !visited[ny][nx] && canvas[ny][nx] == target) { visited[ny][nx] = 1; fstack[top++] = ny * CW + nx; }
        nx = x; ny = y + 1; if (ny < CH  && !visited[ny][nx] && canvas[ny][nx] == target) { visited[ny][nx] = 1; fstack[top++] = ny * CW + nx; }
        nx = x; ny = y - 1; if (ny >= 0  && !visited[ny][nx] && canvas[ny][nx] == target) { visited[ny][nx] = 1; fstack[top++] = ny * CW + nx; }
    }
}

// dithered fg→bg gradient — drag a line to set direction and extent.
// each canvas pixel is projected onto the AB vector; t=0 at A (fg), t=1 at B (bg).
static void cgrad(int ax, int ay, int bx, int by) {
    float dx = bx - ax, dy = by - ay;
    float len2 = dx*dx + dy*dy;
    if (len2 < 1.0f) { for (int y = 0; y < CH; y++) for (int x = 0; x < CW; x++) cset(x, y, fg); return; }
    for (int y = 0; y < CH; y++)
        for (int x = 0; x < CW; x++) {
            float t = ((x - ax)*dx + (y - ay)*dy) / len2;
            if (t < 0.0f) t = 0.0f; if (t > 1.0f) t = 1.0f;
            float thr = (BAYER[y & 3][x & 3] + 0.5f) / 16.0f;
            cset(x, y, t > thr ? bg : fg);
        }
}

static void cspray(int cx, int cy, int draw) {
    int r = brush * 2 + 2, n = brush * 3 + 4;
    for (int i = 0; i < n; i++) {
        float a = (float)rnd(360);
        float rr = rnd_float() * r;
        int x = cx + (int)(cos_deg(a) * rr), y = cy + (int)(sin_deg(a) * rr);
        cset(x, y, pat_at(x, y, draw));
    }
}

static void capture(int x0, int y0, int x1, int y1) {
    int xa = min(x0, x1), xb = max(x0, x1), ya = min(y0, y1), yb = max(y0, y1);
    xa = mid(0, xa, CW - 1); xb = mid(0, xb, CW - 1);
    ya = mid(0, ya, CH - 1); yb = mid(0, yb, CH - 1);
    cbw = min(CBR, xb - xa + 1); cbh = min(CBR, yb - ya + 1);
    for (int y = 0; y < cbh; y++)
        for (int x = 0; x < cbw; x++) cbr[y][x] = canvas[ya + y][xa + x];
    hasbr = true;
}

static void cstamp(int cx, int cy) {
    if (!hasbr) return;
    int x0 = cx - cbw / 2, y0 = cy - cbh / 2;
    for (int y = 0; y < cbh; y++)
        for (int x = 0; x < cbw; x++) {
            int v = cbr[y][x];
            if (v != 0) cset(x0 + x, y0 + y, v);   // black acts as transparent
        }
}

// ── bezier onto canvas ───────────────────────────────────────────────────────
static void cbezier(int x0, int y0, int cx, int cy, int x1, int y1, int draw) {
    float len = fsqrt((float)((cx-x0)*(cx-x0)+(cy-y0)*(cy-y0)))
              + fsqrt((float)((x1-cx)*(x1-cx)+(y1-cy)*(y1-cy)));
    int n = (int)(len * 0.5f); if (n < 4) n = 4; if (n > 200) n = 200;
    float px = (float)x0, py = (float)y0;
    for (int i = 1; i <= n; i++) {
        float t = i / (float)n, mt = 1.0f - t;
        float nx = mt*mt*x0 + 2.0f*mt*t*cx + t*t*x1;
        float ny = mt*mt*y0 + 2.0f*mt*t*cy + t*t*y1;
        cline((int)(px+0.5f), (int)(py+0.5f), (int)(nx+0.5f), (int)(ny+0.5f), draw);
        px = nx; py = ny;
    }
}

// ── polygon fill + commit ────────────────────────────────────────────────────
static void push_undo(void);
static void cpolyfill(const int *xy, int n, int draw) {
    if (n < 3) return;
    int x0 = xy[0], x1 = xy[0], y0 = xy[1], y1 = xy[1];
    for (int i = 1; i < n; i++) {
        if (xy[i*2]   < x0) x0 = xy[i*2];   if (xy[i*2]   > x1) x1 = xy[i*2];
        if (xy[i*2+1] < y0) y0 = xy[i*2+1]; if (xy[i*2+1] > y1) y1 = xy[i*2+1];
    }
    if (x0 < 0) x0 = 0; if (x1 >= CW) x1 = CW - 1;
    if (y0 < 0) y0 = 0; if (y1 >= CH) y1 = CH - 1;
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            bool inside = false;
            for (int i = 0, j = n - 1; i < n; j = i++) {
                float yi = (float)xy[i*2+1], yj = (float)xy[j*2+1], fy = y + 0.5f;
                if ((yi > fy) != (yj > fy))
                    if ((float)x + 0.5f < (xy[j*2] - xy[i*2]) * (fy - yi) / (yj - yi) + xy[i*2])
                        inside = !inside;
            }
            if (inside) cset(x, y, pat_at(x, y, draw));
        }
}

static void commit_poly(int draw) {
    if (poly_n < 2) { poly_n = 0; return; }
    push_undo();
    if (poly_n == 2) {
        cline(poly_pts[0], poly_pts[1], poly_pts[2], poly_pts[3], draw);
    } else if (filled) {
        cpolyfill(poly_pts, poly_n, draw);
    } else {
        for (int i = 0; i < poly_n; i++)
            cline(poly_pts[i*2], poly_pts[i*2+1], poly_pts[((i+1)%poly_n)*2], poly_pts[((i+1)%poly_n)*2+1], draw);
    }
    poly_n = 0;
}

// ── undo ────────────────────────────────────────────────────────────────────
static void push_undo(void) {
    for (int y = 0; y < CH; y++) for (int x = 0; x < CW; x++) undostack[undotop][y][x] = canvas[y][x];
    undotop = (undotop + 1) % UNDO_N;
    if (undocount < UNDO_N) undocount++;
}
static void do_undo(void) {
    if (undocount == 0) return;
    undotop = (undotop - 1 + UNDO_N) % UNDO_N;
    for (int y = 0; y < CH; y++) for (int x = 0; x < CW; x++) canvas[y][x] = undostack[undotop][y][x];
    undocount--;
}

static void flash(const char *m) { msg = m; msgT = 1.4f; }

// ── starter scene (also the thumbnail) — sky gradient, sun, hills, cycling water ──
static void paint_demo(void) {
    mirx = miry = false; patHole = -1; pat = 0x0000;

    // sky: dithered vertical gradient
    fg = CLR_BLUE; bg = CLR_DARK_BLUE; cgrad(0, 0, 0, 96);

    // cycling water band (WATER ramp, flowing diagonally)
    for (int y = 96; y < CH; y++)
        for (int x = 0; x < CW; x++) {
            int k = (x / 3 + (y - 96)) % RAMP_N[1];
            canvas[y][x] = (unsigned char)RAMP_IDX[1][k];
        }

    // hills — pattern-filled (checker green over a darker green)
    pat = 0xA5A5; patHole = CLR_DARK_GREEN;
    coval(-30, 70, 96, 130, CLR_MEDIUM_GREEN, true);
    coval(70, 80, 210, 140, CLR_GREEN, true);
    pat = 0x0000; patHole = -1;

    // sun with a dithered halo
    coval(126, 6, 170, 50, CLR_ORANGE, true);
    pat = 0x8020; patHole = -1;
    coval(116, -4, 180, 60, CLR_LIGHT_YELLOW, false);
    pat = 0x0000;
    coval(132, 12, 164, 44, CLR_YELLOW, true);

    // a cloud or two
    coval(18, 18, 78, 38, CLR_WHITE, true);
    coval(48, 10, 104, 32, CLR_WHITE, true);
    coval(150, 26, 200, 44, CLR_LIGHT_GREY, true);

    // restore working defaults
    fg = CLR_WHITE; bg = CLR_BLACK; pat = 0x0000; patHole = -1;
    tool = T_PENCIL; brush = 1; filled = true;
}

void init(void) {
    fg = CLR_WHITE; bg = CLR_BLACK; tool = T_PENCIL; brush = 1;
    pat = 0x0000; patHole = -1; filled = true; mirx = miry = false;
    cycleOn = true; rampSel = 1; cycSpeed = 5.0f; cycPhase = 0;
    colorkey(-1);

    int n = load_bytes(canvas, sizeof canvas);
    if (n != (int)sizeof canvas) paint_demo();   // first run -> demo scene
}

// ── input ────────────────────────────────────────────────────────────────────
void update(void) {
    M_X = mouse_x(); M_Y = mouse_y();
    curcx = M_X - CANVAS_X; curcy = M_Y - CANVAS_Y;
    curin = curcx >= 0 && curcx < CW && curcy >= 0 && curcy < CH;

    if (cycleOn) cycPhase += cycSpeed * dt();
    if (msgT > 0) msgT -= dt();

    // cancel in-progress polygon when switching tools
    static int prev_tool = T_PENCIL;
    if (tool != prev_tool) { if (prev_tool == T_POLY) poly_n = 0; if (prev_tool == T_BEZIER) bez_phase2 = false; prev_tool = tool; }

    // keyboard shortcuts
    if (keyp('B')) tool = T_PENCIL;
    if (keyp('L')) tool = T_LINE;
    if (keyp('R')) tool = T_RECT;
    if (keyp('O')) tool = T_OVAL;
    if (keyp('F')) tool = T_FILL;
    if (keyp('S')) tool = T_SPRAY;
    if (keyp('I')) tool = T_PICK;
    if (keyp('C')) { tool = T_BRUSH; grabMode = true; }
    if (keyp('D')) tool = T_GRAD;
    if (keyp('P')) tool = T_POLY;
    if (keyp('Z')) tool = T_BEZIER;
    if (keyp('X')) { int t = fg; fg = bg; bg = t; }
    if (keyp('M')) mirx = !mirx;
    if (keyp('H')) patHole = (patHole < 0) ? bg : -1;
    if (keyp('U')) do_undo();
    if (keyp(']')) brush = min(8, brush + 1);
    if (keyp('[')) brush = max(1, brush - 1);
    if (keyp(KEY_ESCAPE)) { if (tool == T_POLY) poly_n = 0; bez_phase2 = false; }

    // ── canvas interaction ──
    if (curin) {
        bool Lp = mouse_pressed(MOUSE_LEFT),  Rp = mouse_pressed(MOUSE_RIGHT);
        bool L  = mouse_down(MOUSE_LEFT),     R  = mouse_down(MOUSE_RIGHT);
        bool Lr = mouse_released(MOUSE_LEFT), Rr = mouse_released(MOUSE_RIGHT);
        int draw = (Rp || R) ? bg : fg;

        switch (tool) {
            case T_PENCIL:
                if (Lp || Rp) { push_undo(); painting = true; lastx = curcx; lasty = curcy; cdab(curcx, curcy, draw); }
                else if (painting && (L || R)) { cline(lastx, lasty, curcx, curcy, draw); lastx = curcx; lasty = curcy; }
                if (Lr || Rr) painting = false;
                break;
            case T_SPRAY:
                if (Lp || Rp) push_undo();
                if (L || R) cspray(curcx, curcy, draw);
                break;
            case T_PICK:
                if (Lp) fg = canvas[curcy][curcx];
                if (Rp) bg = canvas[curcy][curcx];
                break;
            case T_FILL:
                if (Lp || Rp) { push_undo(); cflood(curcx, curcy, draw); }
                break;
            case T_LINE: case T_RECT: case T_OVAL: case T_GRAD:
                if (Lp || Rp) { dragging = true; dsx = curcx; dsy = curcy; dragRight = Rp; }
                break;
            case T_BEZIER:
                if (bez_phase2) {
                    if (Lp) { push_undo(); cbezier(bez_x0, bez_y0, curcx, curcy, bez_x1, bez_y1, bez_right ? bg : fg); bez_phase2 = false; }
                    if (Rp) bez_phase2 = false;
                } else {
                    if (Lp || Rp) { dragging = true; dsx = curcx; dsy = curcy; dragRight = Rp; }
                }
                break;
            case T_POLY:
                if (Lp) {
                    if (poly_n >= 3) {
                        int ddx = curcx - poly_pts[0], ddy = curcy - poly_pts[1];
                        if (ddx*ddx + ddy*ddy <= 25) { commit_poly(fg); break; }
                    }
                    if (poly_n < POLY_MAX) { poly_pts[poly_n*2] = curcx; poly_pts[poly_n*2+1] = curcy; poly_n++; }
                }
                if (Rp) commit_poly(fg);
                break;
            case T_BRUSH:
                if (grabMode) { if (Lp) { dragging = true; dsx = curcx; dsy = curcy; dragRight = false; } }
                else if (Lp || Rp) { push_undo(); cstamp(curcx, curcy); }
                break;
        }
    }

    // commit drag tools on release (release may land outside the canvas)
    if (dragging && (mouse_released(MOUSE_LEFT) || mouse_released(MOUSE_RIGHT))) {
        int ex = mid(0, curcx, CW - 1), ey = mid(0, curcy, CH - 1);
        int draw = dragRight ? bg : fg;
        if (tool == T_BRUSH && grabMode) { capture(dsx, dsy, ex, ey); grabMode = false; flash("brush grabbed"); }
        else if (tool == T_BEZIER) { bez_x0 = dsx; bez_y0 = dsy; bez_x1 = ex; bez_y1 = ey; bez_right = dragRight; bez_phase2 = true; }
        else {
            push_undo();
            if      (tool == T_LINE) cline(dsx, dsy, ex, ey, draw);
            else if (tool == T_RECT) crect(dsx, dsy, ex, ey, draw, filled);
            else if (tool == T_OVAL) coval(dsx, dsy, ex, ey, draw, filled);
            else if (tool == T_GRAD) cgrad(dsx, dsy, ex, ey);
        }
        dragging = false;
    }
}

// ── small UI helpers ─────────────────────────────────────────────────────────
static bool button(int x, int y, int w, int h, const char *label, bool active) {
    bool hot = M_X >= x && M_X < x + w && M_Y >= y && M_Y < y + h;
    rectfill(x, y, w, h, active ? CLR_TRUE_BLUE : hot ? CLR_DARK_GREY : CLR_DARKER_GREY);
    rect(x, y, w, h, active ? CLR_WHITE : hot ? CLR_LIGHT_GREY : CLR_DARK_GREY);
    if (label) {
        int tw = text_width(label);
        print(label, x + (w - tw) / 2, y + (h - 5) / 2, active ? CLR_WHITE : CLR_LIGHT_GREY);
    }
    return hot && mouse_pressed(MOUSE_LEFT);
}

// draw a tool icon centered in (x,y,17,17)
static void tool_icon(int t, int x, int y, int c) {
    int cx = x + 8, cy = y + 8;
    switch (t) {
        case T_PENCIL: line(x + 3, y + 13, x + 12, y + 4, c); line(x + 11, y + 3, x + 13, y + 5, c); pset(x + 3, y + 13, CLR_YELLOW); break;
        case T_LINE:   line(x + 3, y + 13, x + 13, y + 3, c); break;
        case T_RECT:   rect(x + 3, y + 4, 11, 9, c); break;
        case T_OVAL:   oval(cx, cy, 5, 4, c); break;
        case T_FILL:   trifill(x + 4, y + 9, x + 10, y + 3, x + 13, y + 9, c); line(x + 11, y + 10, x + 11, y + 13, CLR_BLUE); pset(x + 11, y + 13, CLR_BLUE); break;
        case T_SPRAY:  for (int i = 0; i < 9; i++) pset(x + 4 + rnd(9), y + 3 + rnd(10), c); break;
        case T_PICK:   line(x + 3, y + 13, x + 9, y + 7, c); line(x + 9, y + 4, x + 13, y + 8, c); pset(x + 3, y + 13, CLR_YELLOW); break;
        case T_BRUSH:  for (int i = 0; i < 11; i += 2) { pset(x + 3 + i, y + 3, c); pset(x + 3 + i, y + 13, c); pset(x + 3, y + 3 + i, c); pset(x + 13, y + 3 + i, c); } break;
        case T_GRAD:   for (int i = 0; i < 11; i++) { int col = (i < 4) ? c : (i < 8) ? CLR_DARK_GREY : CLR_BLACK; line(x+3+i, y+3, x+3+i, y+13, col); } line(x+2,y+13,x+14,y+3,CLR_WHITE); break;
        case T_POLY: {
            int pxy[10] = { x+8,y+2, x+14,y+7, x+12,y+13, x+4,y+13, x+2,y+7 };
            for (int i = 0; i < 5; i++) { int j = (i+1)%5; line(pxy[i*2],pxy[i*2+1],pxy[j*2],pxy[j*2+1],c); }
            break;
        }
        case T_BEZIER:
            bezier(x+2, y+13, x+5, y+2, x+14, y+13, c);
            pset(x+2,  y+13, c); pset(x+14, y+13, c);
            line(x+2, y+13, x+5, y+2, CLR_DARK_GREY);
            break;
    }
}

static void draw_pat_swatch(int patv, int x, int y, int w, int h) {
    int hole = (patHole < 0) ? CLR_DARKER_GREY : patHole;
    for (int yy = 0; yy < h; yy++)
        for (int xx = 0; xx < w; xx++) {
            int i = ((yy & 3) * 4) + (xx & 3);
            int on = (patv >> (15 - i)) & 1;
            pset(x + xx, y + yy, on ? hole : fg);
        }
}

// ── render ────────────────────────────────────────────────────────────────────
void draw(void) {
    cls(CLR_DARKER_PURPLE);

    // canvas frame
    rect(CANVAS_X - 1, CANVAS_Y - 1, CW + 2, CH + 2, CLR_BLACK);

    // replay the canvas through the (optionally cycling) palette
    if (cycleOn) {
        int n = RAMP_N[rampSel];
        int sh = ((int)cycPhase) % n; if (sh < 0) sh += n;
        for (int i = 0; i < n; i++) pal(RAMP_IDX[rampSel][i], RAMP_IDX[rampSel][(i + sh) % n]);
    }
    for (int y = 0; y < CH; y++)
        for (int x = 0; x < CW; x++) pset(CANVAS_X + x, CANVAS_Y + y, canvas[y][x]);
    pal_reset();

    // ── live tool preview (clipped to the canvas) ──
    clip(CANVAS_X, CANVAS_Y, CW, CH);
    if (dragging) {
        int ex = mid(0, curcx, CW - 1), ey = mid(0, curcy, CH - 1);
        int sx = CANVAS_X + dsx, sy = CANVAS_Y + dsy, ux = CANVAS_X + ex, uy = CANVAS_Y + ey;
        int pc = blink(8) ? CLR_WHITE : CLR_BLACK;
        if      (tool == T_LINE) line(sx, sy, ux, uy, pc);
        else if (tool == T_RECT) rect(min(sx, ux), min(sy, uy), abs(ux - sx) + 1, abs(uy - sy) + 1, pc);
        else if (tool == T_OVAL) oval((sx + ux) / 2, (sy + uy) / 2, abs(ux - sx) / 2, abs(uy - sy) / 2, pc);
        else if (tool == T_GRAD) line(sx, sy, ux, uy, pc);
        else if (tool == T_BRUSH && grabMode) rect(min(sx, ux), min(sy, uy), abs(ux - sx) + 1, abs(uy - sy) + 1, CLR_GREEN);
    }
    if (curin && !dragging && tool != T_POLY) {
        int cx = CANVAS_X + curcx, cy = CANVAS_Y + curcy, r = brush;
        int pc = blink(16) ? CLR_WHITE : CLR_BLACK;
        rect(cx - r, cy - r, 2 * r + 1, 2 * r + 1, pc);
    }
    // bezier phase 2: live curve + handle lines
    if (tool == T_BEZIER && bez_phase2) {
        int ax = CANVAS_X + bez_x0, ay = CANVAS_Y + bez_y0;
        int bx = CANVAS_X + bez_x1, by = CANVAS_Y + bez_y1;
        int mx = CANVAS_X + curcx,  my = CANVAS_Y + curcy;
        line(ax, ay, mx, my, CLR_DARK_GREY);
        line(bx, by, mx, my, CLR_DARK_GREY);
        bezier(ax, ay, mx, my, bx, by, blink(8) ? CLR_WHITE : CLR_LIGHT_GREY);
        pset(ax, ay, CLR_YELLOW); pset(bx, by, CLR_YELLOW);
        pset(mx, my, CLR_WHITE);
    }
    // polygon tool: in-progress vertex chain
    if (tool == T_POLY && poly_n > 0) {
        for (int i = 0; i + 1 < poly_n; i++)
            line(CANVAS_X + poly_pts[i*2], CANVAS_Y + poly_pts[i*2+1],
                 CANVAS_X + poly_pts[i*2+2], CANVAS_Y + poly_pts[i*2+3], CLR_WHITE);
        if (curin && blink(8))
            line(CANVAS_X + poly_pts[(poly_n-1)*2], CANVAS_Y + poly_pts[(poly_n-1)*2+1],
                 CANVAS_X + curcx, CANVAS_Y + curcy, CLR_LIGHT_GREY);
        for (int i = 0; i < poly_n; i++)
            pset(CANVAS_X + poly_pts[i*2], CANVAS_Y + poly_pts[i*2+1], i == 0 ? CLR_YELLOW : CLR_WHITE);
        if (curin && poly_n >= 3) {
            int ddx = curcx - poly_pts[0], ddy = curcy - poly_pts[1];
            if (ddx*ddx + ddy*ddy <= 25)
                rect(CANVAS_X + poly_pts[0] - 3, CANVAS_Y + poly_pts[1] - 3, 7, 7, CLR_YELLOW);
        }
    }
    clip(0, 0, 0, 0);

    // ── title ──
    print("DELUXE PAINT", CANVAS_X, 3, CLR_LIGHT_PEACH);

    // ── tool palette (left) ──
    for (int t = 0; t < T_COUNT; t++) {
        int x = TBX + (t % 2) * (TBW + 1), y = TBY + (t / 2) * (TBH + 1);
        bool active = (tool == t), hot = M_X >= x && M_X < x + TBW && M_Y >= y && M_Y < y + TBH;
        rectfill(x, y, TBW, TBH, active ? CLR_TRUE_BLUE : hot ? CLR_DARK_GREY : CLR_DARKER_GREY);
        rect(x, y, TBW, TBH, active ? CLR_WHITE : CLR_DARKER_GREY);
        tool_icon(t, x, y, active ? CLR_WHITE : CLR_LIGHT_GREY);
        if (hot && mouse_pressed(MOUSE_LEFT)) { if (t != T_POLY) poly_n = 0; tool = t; if (t == T_BRUSH) grabMode = !hasbr; }
    }
    // brush size under the toolbar (BEZIER ends at y=121, so start at 125)
    print("SIZE", TBX, 125, CLR_DARK_GREY);
    if (button(TBX, 133, 16, 13, "-", false)) brush = max(1, brush - 1);
    if (button(TBX + 19, 133, 16, 13, "+", false)) brush = min(8, brush + 1);
    print(str("%d", brush), TBX + 15, 149, CLR_WHITE);

    // ── sidebar (right) ──
    int sx = SBX;
    // colors
    print("COLORS", sx, 14, CLR_LIGHT_GREY);
    rectfill(sx, 24, 30, 20, fg); rect(sx, 24, 30, 20, CLR_WHITE);
    rectfill(sx + 34, 24, 30, 20, bg); rect(sx + 34, 24, 30, 20, CLR_DARK_GREY);
    if (button(sx + 70, 24, 34, 20, "SWAP", false)) { int t = fg; fg = bg; bg = t; }

    // patterns
    print("PATTERN", sx, 50, CLR_LIGHT_GREY);
    for (int i = 0; i < NPAT; i++) {
        int px = sx + (i % 5) * 20, py = 60 + (i / 5) * 16;
        bool sel = (pat == PATS[i]), hot = M_X >= px && M_X < px + 18 && M_Y >= py && M_Y < py + 13;
        draw_pat_swatch(PATS[i], px, py, 18, 13);
        rect(px - 1, py - 1, 20, 15, sel ? CLR_WHITE : hot ? CLR_LIGHT_GREY : CLR_DARK_GREY);
        if (hot && mouse_pressed(MOUSE_LEFT)) pat = PATS[i];
    }
    if (button(sx, 94, 64, 12, patHole < 0 ? "TRANSP" : "OPAQUE", patHole >= 0))
        patHole = (patHole < 0) ? bg : -1;

    // shape fill mode
    if (button(sx + 70, 94, 34, 12, filled ? "FILL" : "LINE", false)) filled = !filled;

    // mirror
    print("MIRROR", sx, 112, CLR_LIGHT_GREY);
    if (button(sx + 44, 110, 26, 12, "X", mirx)) mirx = !mirx;
    if (button(sx + 72, 110, 26, 12, "Y", miry)) miry = !miry;

    // color cycling — the showpiece
    print("CYCLE", sx, 144, CLR_LIGHT_GREY);
    if (button(sx + 44, 142, 54, 12, cycleOn ? "ON" : "OFF", cycleOn)) cycleOn = !cycleOn;
    for (int i = 0; i < RAMPS; i++) {
        int bx = sx + i * 35;
        if (button(bx, 158, 32, 12, RAMP_NAME[i], rampSel == i)) rampSel = i;
    }
    // live ramp preview
    for (int i = 0; i < RAMP_N[rampSel]; i++) {
        int n = RAMP_N[rampSel], sh = ((int)cycPhase) % n; if (sh < 0) sh += n;
        rectfill(sx + i * 14, 174, 13, 8, RAMP_IDX[rampSel][(i + sh) % n]);
    }
    print("SPD", sx, 186, CLR_DARK_GREY);
    if (button(sx + 24, 184, 14, 11, "-", false)) cycSpeed = max(1.0f, cycSpeed - 1);
    if (button(sx + 41, 184, 14, 11, "+", false)) cycSpeed = min(20.0f, cycSpeed + 1);
    print(str("%d", (int)cycSpeed), sx + 60, 186, CLR_WHITE);

    // captured custom brush preview (left column, under SIZE)
    if (hasbr) {
        print("STAMP", TBX, 163, CLR_DARK_GREY);
        int bx = TBX, by = 173;
        int pw = min(cbw, 36), ph = min(cbh, 32);
        rect(bx - 1, by - 1, pw + 2, ph + 2, CLR_DARK_GREY);
        for (int y = 0; y < ph; y++)
            for (int x = 0; x < pw; x++) {
                int v = cbr[y][x];
                if (v != 0) pset(bx + x, by + y, v);
            }
    }

    // ── action buttons under the canvas ──
    const char *acts[5] = { "UNDO", "CLEAR", "SAVE", "LOAD", "GRAB" };
    for (int i = 0; i < 5; i++) {
        int x = CANVAS_X + i * 39;
        if (button(x, 164, 37, 14, acts[i], i == 4 && grabMode)) {
            if (i == 0) do_undo();
            else if (i == 1) { push_undo(); for (int y = 0; y < CH; y++) for (int xx = 0; xx < CW; xx++) canvas[y][xx] = bg; }
            else if (i == 2) { save_bytes(canvas, sizeof canvas); flash("saved"); }
            else if (i == 3) { int n = load_bytes(canvas, sizeof canvas); if (n == (int)sizeof canvas) flash("loaded"); else flash("nothing saved"); }
            else { tool = T_BRUSH; grabMode = true; }
        }
    }

    // ── status line ──
    print(str("%s  size %d  %s%s", TOOLNAME[tool], brush,
              mirx ? "mirX " : "", miry ? "mirY" : ""), CANVAS_X, 184, CLR_LIGHT_GREY);
    if (curin) print_right(str("%d,%d", curcx, curcy), CANVAS_X + CW, 184, CLR_DARK_GREY);
    if (msgT > 0) print_right(msg, CANVAS_X + CW, 196, CLR_GREEN);

    // ── palette strip (bottom) ──
    rectfill(0, 208, SCREEN_W, 32, CLR_BLACK);
    for (int i = 0; i < 32; i++) {
        int x = i * 12, y = 214;
        bool hot = M_X >= x && M_X < x + 11 && M_Y >= y && M_Y < y + 22;
        rectfill(x, y, 11, 22, i);
        if (i == fg) rect(x, y, 11, 22, CLR_WHITE);
        else if (i == bg) rect(x, y, 11, 22, CLR_LIGHT_GREY);
        else if (hot) rect(x, y, 11, 22, CLR_DARK_GREY);
        if (hot) {
            if (mouse_pressed(MOUSE_LEFT))  fg = i;
            if (mouse_pressed(MOUSE_RIGHT)) bg = i;
        }
    }

}
