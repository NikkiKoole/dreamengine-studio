/* de:meta
{
  "slug": "extrudetext",
  "title": "extrude text",
  "status": "active",
  "created": "2026-07-16",
  "kind": [
    "tech-demo",
    "toy"
  ],
  "teaches": [],
  "genre": "lab",
  "description": {
    "summary": "A Hotline-Miami block-letter title lab: the same word drawn N times, each copy stepped along an offset vector so the hard-edged copies fill in and the type reads as a solid extruded block. One toggle switches the face between the bitmap font (print_scaled, zero assets) and a baked TTF display face (Bungee, sspr) — the extrude, the orbiting spin, and the per-layer colour STEP work in both.",
    "detail": "The whole trick is one loop: draw the word N times back-to-front, each copy at (x+i*dx, y+i*dy), then the bright face last on top. Solid hard-edged copies fill the gaps, so the letters gain physical thickness — the between-level GET READY / PUSH START look. Two renderers behind one set of knobs: BITMAP mode loops print_scaled() (no assets, any string); TTF mode sspr()s a flat single-ink silhouette baked from Bungee (extrudetext.cart.js, tools/font-bake.js) and pal()-recolours it. Shared experiments: SPIN — a timer orbits the offset vector so the light source appears to move and the extrude sweeps around; COLOURSTEP — each layer takes the next colour down a curated ramp (pink->purple->black etc) so the block reads lit-to-shadow instead of one flat dark. Everything is a live knob.",
    "controls": "M toggle bitmap/TTF face · UP/DOWN depth · LEFT/RIGHT offset length · Z spin on/off · hold [ ] spin speed (past 0 = reverse) · X cycle word · C cycle colour ramp"
  }
}
de:meta */
// EXTRUDE TEXT — the stacked-shadow block-letter effect, two renderers, one
// set of knobs.
//
// The trick is one loop: draw the same word N times, each copy stepped one more
// step along a fixed (dx,dy) vector, back-to-front, then the bright FACE last on
// top. Hard-edged solid copies -> the gaps fill in -> the letters read as an
// extruded 3D block. This is the Hotline Miami between-level title look.
//
//   for i in depth..1:  draw(word, x+i*dx, y+i*dy, ramp_col(i))   // extrude
//   draw(word, x, y, face)                                        // face, last
//
//   • MODE  — M toggles the face: BITMAP (print_scaled, no assets, any string)
//             vs TTF (a flat silhouette baked from Bungee, sspr + pal recolour).
//   • SPIN  — a timer advances an angle; dx=cos*len, dy=sin*len, so the extrude
//             direction orbits (a moving light source). Hold [ ] to change speed.
//   • STEP  — each layer takes the next colour down a ramp, turning the flat
//             dark shadow into a lit-to-shadow gradient.

#include "studio.h"
#include <math.h>

#define STATE struct GameState
#define S     ((STATE *)de_state(sizeof(STATE)))

enum { MODE_BITMAP, MODE_TTF };

STATE {
    int    mode;       // MODE_BITMAP or MODE_TTF
    int    depth;      // extrude copies
    float  len;        // offset length in px per copy
    float  ang;        // current extrude-vector angle (radians)
    bool   spin;       // orbit the vector on a timer?
    float  spin_spd;   // radians per frame when spinning
    int    word;       // which word (indexes both WORDS[] and banners[])
    int    ramp;       // which colour ramp
};

#define INK 7          // the flat silhouette ink we recolour per layer (TTF mode)

// BITMAP mode: the strings, parallel to the TTF banners below (same index = same word)
static const char *WORDS[] = { "DREAM", "HOTLINE", "1985" };
#define BMP_SCALE 4    // print_scaled scale of the bitmap face

// TTF mode: baked banner regions on the sheet (see extrudetext.cart.js)
typedef struct { int sx, sy, sw, sh; } Banner;
static const Banner banners[] = {
    {   0,  0, 128, 48 },   // DREAM
    {   0, 48, 128, 48 },   // HOTLINE
    {   0, 96, 128, 32 },   // 1985
};
#define NWORDS ((int)(sizeof(banners)/sizeof(banners[0])))
#define ZOOM 1.5f          // dest scale of the 128px banner (128*1.5 = 192, fits 320)

// colour ramps: near-face (bright) -> deepest (dark). `face` is the front copy,
// the extrude steps DOWN the rest as it recedes. Each ends dark to ground it.
typedef struct { int face; int n; int col[6]; } Ramp;
static const Ramp ramps[] = {
    { CLR_WHITE, 5, { CLR_PINK,        CLR_MAUVE,     CLR_DARK_PURPLE, CLR_DARKER_PURPLE, CLR_BLACK } }, // miami
    { CLR_WHITE, 5, { CLR_LIGHT_YELLOW,CLR_YELLOW,    CLR_ORANGE,      CLR_DARK_RED,      CLR_BROWNISH_BLACK } }, // fire
    { CLR_WHITE, 5, { CLR_LIME_GREEN,  CLR_GREEN,     CLR_DARK_GREEN,  CLR_DARK_BLUE,     CLR_BLACK } }, // toxic
    { CLR_WHITE, 5, { CLR_LIGHT_GREY,  CLR_TRUE_BLUE, CLR_DARK_BLUE,   CLR_DARKER_BLUE,   CLR_BLACK } }, // ice
};
#define NRAMP ((int)(sizeof(ramps)/sizeof(ramps[0])))

void init(void) {
    colorkey(0);       // baked silhouettes (TTF mode): palette 0 is the transparent bg
    S->mode = MODE_TTF;
    S->depth = 10;
    S->len = 1.4f;
    S->ang = 0.9f;     // a nice down-right start angle
    S->spin = true;
    S->spin_spd = 0.06f;
    S->word = 0;
    S->ramp = 0;
}

// colour for extrude layer i (1..depth): step down the ramp by depth fraction.
static int ramp_col(const Ramp *r, int i, int depth) {
    if (depth <= 1) return r->col[r->n - 1];
    float t = (float)(i - 1) / (float)(depth - 1);   // 0 = nearest face, 1 = deepest
    int idx = (int)(t * (r->n - 1) + 0.5f);
    if (idx < 0) idx = 0; if (idx >= r->n) idx = r->n - 1;
    return r->col[idx];
}

void update(void) {
    if (keyp('m') || keyp('M')) S->mode = (S->mode + 1) % 2;

    if (btnp(0, BTN_UP))    S->depth++;
    if (btnp(0, BTN_DOWN) && S->depth > 0) S->depth--;
    if (btnp(0, BTN_LEFT)  && S->len > 0.2f) S->len -= 0.2f;
    if (btnp(0, BTN_RIGHT)) S->len += 0.2f;

    if (btnp(0, BTN_A)) S->spin = !S->spin;                 // Z
    if (btnp(0, BTN_B)) S->word = (S->word + 1) % NWORDS;   // X
    if (keyp('c') || keyp('C')) S->ramp = (S->ramp + 1) % NRAMP;

    // spin speed: hold [ slower/reverse · ] faster. Negative = orbit the other way.
    if (key('[')) S->spin_spd -= 0.01f;
    if (key(']')) S->spin_spd += 0.01f;
    if (S->spin_spd < -0.4f) S->spin_spd = -0.4f;
    if (S->spin_spd >  0.4f) S->spin_spd =  0.4f;

    if (S->spin) S->ang += S->spin_spd;
}

// BITMAP face: loop print_scaled() back-to-front, then the face.
static void draw_bitmap(const Ramp *r, float dx, float dy) {
    const char *word = WORDS[S->word];
    int w  = text_width(word) * BMP_SCALE;
    int th = 8 * BMP_SCALE;
    int fx = (SCREEN_W - w)  / 2 - (int)(S->depth * dx) / 2;
    int fy = (SCREEN_H - th) / 2 - (int)(S->depth * dy) / 2;
    for (int i = S->depth; i >= 1; i--)
        print_scaled(word, fx + (int)(i * dx), fy + (int)(i * dy), ramp_col(r, i, S->depth), BMP_SCALE);
    print_scaled(word, fx, fy, r->face, BMP_SCALE);
}

// TTF face: sspr the flat silhouette back-to-front, pal()-recoloured per layer.
static void draw_ttf(const Ramp *r, float dx, float dy) {
    const Banner *b = &banners[S->word];
    int dw = (int)(b->sw * ZOOM), dh = (int)(b->sh * ZOOM);
    int fx = (SCREEN_W - dw) / 2 - (int)(S->depth * dx) / 2;
    int fy = (SCREEN_H - dh) / 2 - (int)(S->depth * dy) / 2;
    for (int i = S->depth; i >= 1; i--) {
        pal(INK, ramp_col(r, i, S->depth));
        sspr(b->sx, b->sy, b->sw, b->sh, fx + (int)(i * dx), fy + (int)(i * dy), dw, dh);
    }
    pal(INK, r->face);
    sspr(b->sx, b->sy, b->sw, b->sh, fx, fy, dw, dh);
    pal_reset();
}

void draw(void) {
    cls(CLR_DARKER_BLUE);

    const Ramp *r = &ramps[S->ramp];
    float dx = cosf(S->ang) * S->len;
    float dy = sinf(S->ang) * S->len;

    if (S->mode == MODE_TTF) draw_ttf(r, dx, dy);
    else                     draw_bitmap(r, dx, dy);

    // ── knob readout ──
    font(FONT_SMALL);
    int y = 4;
    print(str("mode %s  (M)", S->mode == MODE_TTF ? "TTF" : "bitmap"), 4, y, CLR_LIGHT_GREY); y += 8;
    print(str("depth %d  (UP/DOWN)   len %.1f  (LEFT/RIGHT)", S->depth, S->len), 4, y, CLR_LIGHT_GREY); y += 8;
    print(str("spin %s %.3f  (Z, hold [ ])", S->spin ? "on" : "off", S->spin_spd), 4, y, CLR_LIGHT_GREY); y += 8;
    print("word X   ramp C", 4, y, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);
}
