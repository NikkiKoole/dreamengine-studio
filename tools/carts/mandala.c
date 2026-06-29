/* de:meta
{
  "title": "mandala pad",
  "status": "active",
  "created": "2026-06-03",
  "kind": [
    "toy"
  ],
  "teaches": [
    "radial-symmetry"
  ],
  "lineage": "A generative drawing instrument — the visual cousin of the synth carts; one gesture replicated across N radial sectors (with optional reflection) blooms into a kaleidoscope, RAINBOW mode flowing colour along the stroke.",
  "description": "A generative visual instrument — the drawing cousin of the synth carts. You don't place pixels by hand (that's Deluxe Paint); you make ONE gesture and a kaleidoscope does the rest. Every stroke is mirrored across N radial sectors (and optionally reflected), so a careless squiggle blooms into a symmetric mandala — and in RAINBOW mode the colour flows along the stroke, so it's almost impossible to make something ugly. LEFT-drag paints, RIGHT-drag erases, wheel/[ ] sizes the brush, LEFT/RIGHT change the sector count, M mirror, V colour mode (solid/rainbow/rings), G guides, H hides the UI for a clean screenshot, C clear, N new, S save."
}
de:meta */
#include "studio.h"

// ── MANDALA PAD ───────────────────────────────────────────────────────────
// A generative *visual instrument* — the drawing cousin of the synth carts.
// You don't place pixels one at a time (that's Deluxe Paint); you make ONE
// gesture and the kaleidoscope does the rest. Every stroke is mirrored across
// N radial sectors (and optionally reflected), so a single careless squiggle
// blooms into a symmetric mandala. Drag in RAINBOW mode and the colour flows
// along the stroke — instant, hard-to-make-ugly art.
//
//   LEFT-drag   paint        RIGHT-drag  erase
//   wheel / [ ] brush size   LEFT/RIGHT  sectors (symmetry)
//   M mirror    V colour mode (solid / rainbow / rings)   G guides
//   H hide UI (clean canvas for a screenshot)   C clear   N new   S save
//   in SOLID mode, click the top palette strip to pick a colour
//
// The art lives in our own `art[]` buffer (palette indices), replayed every
// frame; the UI is drawn fresh on top so hiding it leaves the canvas pristine.

#define BG       CLR_BLACK
#define CX       (SCREEN_W / 2)
#define CY       (SCREEN_H / 2)
#define MAX_SYM  16                 // up to 16 sectors → 32 mirrored copies

enum { MODE_SOLID, MODE_RAINBOW, MODE_RINGS, MODE_COUNT };
static const char *MODE_NAME[MODE_COUNT] = { "SOLID", "RAINBOW", "RINGS" };

// a pleasing hue loop through the vivid end of the palette (red→…→purple→red)
#define NSPEC 14
static const int SPECTRUM[NSPEC] = {
    CLR_RED, CLR_DARK_ORANGE, CLR_ORANGE, CLR_YELLOW, CLR_LIGHT_YELLOW,
    CLR_LIME_GREEN, CLR_GREEN, CLR_MEDIUM_GREEN, CLR_BLUE, CLR_TRUE_BLUE,
    CLR_INDIGO, CLR_MAUVE, CLR_PINK, CLR_DARK_PURPLE,
};

// ── state ───────────────────────────────────────────────────────────────────
static unsigned char art[SCREEN_H][SCREEN_W];
static int   sym    = 6;            // radial sectors
static bool  mirror = true;         // reflect within each sector (true kaleidoscope)
static int   brush  = 2;            // dab radius+1 (1 = single pixel)
static int   mode   = MODE_RAINBOW;
static int   fg     = CLR_WHITE;
static bool  guides = false;
static bool  ui     = true;
static float colorPhase = 0;        // advances as you drag, drives RAINBOW

// precomputed symmetry transforms (rotation + optional reflection)
static float cosT[MAX_SYM * 2], sinT[MAX_SYM * 2];
static bool  mirT[MAX_SYM * 2];
static int   nT = 0;

// drag bookkeeping
static bool  painting = false;
static int   lastx, lasty;

static const char *msg = ""; static float msgT = 0;
static void flash(const char *m) { msg = m; msgT = 1.4f; }

// ── symmetry ─────────────────────────────────────────────────────────────────
static void recompute_syms(void) {
    float step = 360.0f / sym;
    nT = 0;
    for (int i = 0; i < sym; i++) {
        float a = i * step;
        cosT[nT] = cos_deg(a); sinT[nT] = sin_deg(a); mirT[nT] = false; nT++;
        if (mirror) { cosT[nT] = cos_deg(a); sinT[nT] = sin_deg(a); mirT[nT] = true; nT++; }
    }
}

// ── canvas writers ────────────────────────────────────────────────────────────
static void dab(int x, int y, int col) {
    int r = brush - 1;
    if (r <= 0) { if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H) art[y][x] = (unsigned char)col; return; }
    for (int dy = -r; dy <= r; dy++)
        for (int dx = -r; dx <= r; dx++) {
            if (dx * dx + dy * dy > r * r + 1) continue;
            int px = x + dx, py = y + dy;
            if (px >= 0 && px < SCREEN_W && py >= 0 && py < SCREEN_H) art[py][px] = (unsigned char)col;
        }
}

static void dab_line(int x0, int y0, int x1, int y1, int col) {
    int dx = abs(x1 - x0), dy = -abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy;
    while (1) {
        dab(x0, y0, col);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

// draw one segment, replicated across every symmetry transform
static void stroke(float ax, float ay, float bx, float by, int col) {
    float arx = ax - CX, ary = ay - CY, brx = bx - CX, bry = by - CY;
    for (int t = 0; t < nT; t++) {
        float a0x = arx, a0y = ary, b0x = brx, b0y = bry;
        if (mirT[t]) { a0x = -a0x; b0x = -b0x; }
        float c = cosT[t], s = sinT[t];
        int tax = CX + (int)(a0x * c - a0y * s + 0.5f), tay = CY + (int)(a0x * s + a0y * c + 0.5f);
        int tbx = CX + (int)(b0x * c - b0y * s + 0.5f), tby = CY + (int)(b0x * s + b0y * c + 0.5f);
        dab_line(tax, tay, tbx, tby, col);
    }
}

// colour for a segment, by the active mode
static int color_for(float ax, float ay, float bx, float by) {
    if (mode == MODE_SOLID) return fg;
    if (mode == MODE_RINGS) {
        float r = distance((int)((ax + bx) / 2), (int)((ay + by) / 2), CX, CY);
        int idx = (int)(r * 0.10f) % NSPEC;
        return SPECTRUM[idx];
    }
    return SPECTRUM[((int)colorPhase) % NSPEC];   // RAINBOW
}

// stroke a closed loop (a petal) centred at (ox,oy) — the symmetry repeats it
// into a ring of petals around the canvas centre
static void petal(float ox, float oy, float rad) {
    float px = ox + rad, py = oy;
    for (int a = 15; a <= 360; a += 15) {
        float x = ox + cos_deg(a) * rad, y = oy + sin_deg(a) * rad;
        stroke(px, py, x, y, color_for(px, py, x, y));
        px = x; py = y;
    }
}

// ── starter mandala (also the thumbnail) — nested rings of petals ─────────────
static void paint_demo(void) {
    for (int y = 0; y < SCREEN_H; y++) for (int x = 0; x < SCREEN_W; x++) art[y][x] = BG;
    int save_brush = brush, save_sym = sym; bool save_mir = mirror; int save_mode = mode;
    sym = 6; mirror = true; brush = 2; mode = MODE_RINGS; recompute_syms();

    petal(CX, CY, 12);                       // tight central rosette
    for (int k = 0; k < 4; k++)              // outward rings of petals
        petal(CX + 28 + k * 22, CY, 15 + k * 3);

    brush = save_brush; sym = save_sym; mirror = save_mir; mode = save_mode; recompute_syms();
    colorPhase = 0;
}

void init(void) {
    colorkey(-1);
    recompute_syms();
    int n = load_bytes(art, sizeof art);
    if (n != (int)sizeof art) paint_demo();   // first run → starter mandala
}

// ── input ────────────────────────────────────────────────────────────────────
void update(void) {
    if (msgT > 0) msgT -= dt();

    // symmetry
    if (keyp(KEY_LEFT))  { sym = max(1, sym - 1); recompute_syms(); }
    if (keyp(KEY_RIGHT)) { sym = min(MAX_SYM, sym + 1); recompute_syms(); }
    if (keyp('M')) { mirror = !mirror; recompute_syms(); }

    // brush
    float w = mouse_wheel();
    if (w > 0 || keyp(']')) brush = min(12, brush + 1);
    if (w < 0 || keyp('[')) brush = max(1,  brush - 1);

    // modes / view
    if (keyp('V')) mode = (mode + 1) % MODE_COUNT;
    if (keyp('G')) guides = !guides;
    if (keyp('H')) ui = !ui;
    if (keyp('C')) { for (int y = 0; y < SCREEN_H; y++) for (int x = 0; x < SCREEN_W; x++) art[y][x] = BG; flash("cleared"); }
    if (keyp('N')) { paint_demo(); flash("new"); }
    if (keyp('S')) { save_bytes(art, sizeof art); flash("saved"); }

    int mx = mouse_x(), my = mouse_y();
    bool inHud = ui && my >= 180;          // don't paint through the bottom bar
    bool inPal = ui && my < 6;             // …or the top palette strip

    // palette pick (SOLID mode)
    if (inPal && mode == MODE_SOLID && mouse_down(MOUSE_LEFT)) fg = mid(0, mx * 32 / SCREEN_W, 31);

    bool L = mouse_down(MOUSE_LEFT), R = mouse_down(MOUSE_RIGHT);
    if (!inHud && !inPal && (L || R)) {
        int col = R ? BG : color_for(lastx, lasty, mx, my);
        if (!painting) { painting = true; lastx = mx; lasty = my; dab(mx, my, col); }
        else {
            stroke(lastx, lasty, mx, my, col);
            if (mode == MODE_RAINBOW && !R) colorPhase += distance(lastx, lasty, mx, my) * 0.12f;
            lastx = mx; lasty = my;
        }
    } else painting = false;
}

// ── small UI helper ────────────────────────────────────────────────────────────
static int M_X, M_Y;
static bool button(int x, int y, int w, int h, const char *label, bool active) {
    bool hot = M_X >= x && M_X < x + w && M_Y >= y && M_Y < y + h;
    rectfill(x, y, w, h, active ? CLR_TRUE_BLUE : hot ? CLR_DARK_GREY : CLR_DARKER_GREY);
    rect(x, y, w, h, active ? CLR_WHITE : CLR_DARK_GREY);
    int tw = text_width(label);
    print(label, x + (w - tw) / 2, y + (h - 5) / 2, active ? CLR_WHITE : CLR_LIGHT_GREY);
    return hot && mouse_pressed(MOUSE_LEFT);
}

// ── render ────────────────────────────────────────────────────────────────────
void draw(void) {
    cls(BG);
    // replay the art (skip background — cls already painted it)
    for (int y = 0; y < SCREEN_H; y++)
        for (int x = 0; x < SCREEN_W; x++)
            if (art[y][x] != BG) pset(x, y, art[y][x]);

    if (!ui) return;   // clean canvas for screenshots

    M_X = mouse_x(); M_Y = mouse_y();

    // sector guides
    if (guides) {
        float step = 360.0f / sym;
        for (int i = 0; i < sym; i++) {
            float a = i * step;
            line(CX, CY, CX + (int)(cos_deg(a) * 200), CY + (int)(sin_deg(a) * 200), CLR_DARKER_GREY);
        }
    }

    // live brush cursor
    if (M_Y >= 6 && M_Y < 180) {
        int pc = blink(16) ? CLR_WHITE : CLR_DARK_GREY;
        circ(M_X, M_Y, brush, pc);
    }

    // top palette strip (only meaningful in SOLID mode, but handy to see)
    if (mode == MODE_SOLID) {
        for (int i = 0; i < 32; i++) {
            int x = i * SCREEN_W / 32, w = (i + 1) * SCREEN_W / 32 - x;
            rectfill(x, 0, w, 6, i);
            if (i == fg) rect(x, 0, w, 6, CLR_WHITE);
        }
    }

    print("MANDALA", 3, 9, CLR_LIGHT_PEACH);
    if (msgT > 0) print_right(msg, SCREEN_W - 3, 9, CLR_GREEN);

    // ── bottom bar ──
    rectfill(0, 180, SCREEN_W, 20, CLR_BLACK);
    font(FONT_SMALL);
    int y0 = 182, y1 = 191, h = 8;

    // row 1 — symmetry, mirror, mode, guides
    if (button(2,  y0, 12, h, "-", false)) { sym = max(1, sym - 1); recompute_syms(); }
    print_centered(str("%d", sym), 22, y0 + 2, CLR_WHITE);
    if (button(30, y0, 12, h, "+", false)) { sym = min(MAX_SYM, sym + 1); recompute_syms(); }
    if (button(46, y0, 34, h, mirror ? "MIRROR" : "single", mirror)) { mirror = !mirror; recompute_syms(); }
    if (button(84, y0, 52, h, MODE_NAME[mode], false)) mode = (mode + 1) % MODE_COUNT;
    if (button(140, y0, 34, h, "GUIDE", guides)) guides = !guides;
    if (button(178, y0, 26, h, "HIDE", false)) ui = false;

    // row 2 — brush, clear, new, save, load
    print("BRUSH", 2, y1 + 2, CLR_LIGHT_GREY);
    if (button(34, y1, 12, h, "-", false)) brush = max(1, brush - 1);
    print_centered(str("%d", brush), 54, y1 + 2, CLR_WHITE);
    if (button(62, y1, 12, h, "+", false)) brush = min(12, brush + 1);
    if (button(84,  y1, 34, h, "CLEAR", false)) { for (int y = 0; y < SCREEN_H; y++) for (int x = 0; x < SCREEN_W; x++) art[y][x] = BG; }
    if (button(122, y1, 28, h, "NEW", false)) { paint_demo(); flash("new"); }
    if (button(154, y1, 28, h, "SAVE", false)) { save_bytes(art, sizeof art); flash("saved"); }
    if (button(186, y1, 28, h, "LOAD", false)) { if (load_bytes(art, sizeof art) == (int)sizeof art) flash("loaded"); else flash("none"); }
    font(FONT_NORMAL);
}
