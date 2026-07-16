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
    "summary": "A Hotline-Miami block-letter title effect: the same word drawn N times, each copy stepped one pixel along a fixed (dx,dy) vector, so the hard-edged copies fill in and the type reads as a solid extruded block receding into the screen.",
    "detail": "The simplest version of the stacked-shadow idea — no baked assets, just print_scaled() in a loop. Draw the string N times back-to-front, each copy at (x+i*dx, y+i*dy), then the bright face last on top. Because the copies are solid (no blur) the gaps fill in and the letters gain physical thickness — the between-level GET READY / PUSH START look. Everything is a live knob so you can scrub the feel: depth N, the offset vector, and a colour ramp that darkens each layer toward the back (or keeps it one flat colour, Hotline-Miami style). Next slice swaps the bitmap font for a baked TTF display face (see highnoon / font-bake.js).",
    "controls": "UP/DOWN depth · LEFT/RIGHT & the offset keys shift the extrude vector · Z ramp on/off · X cycle word · C cycle palette"
  }
}
de:meta */
// EXTRUDE TEXT — the stacked-shadow block-letter effect, simplest form.
//
// The whole trick is one loop: draw the same string N times, each copy stepped
// one more pixel along a fixed (dx,dy) vector, back-to-front, then the bright
// FACE last on top. Hard-edged solid copies → the gaps fill in → the letters
// read as an extruded 3D block. This is the Hotline Miami between-level title
// look ("GET READY", "PUSH START"). No baked assets — just print_scaled().
//
//   for i in N..1:  print_scaled(word, x + i*dx, y + i*dy, backCol, scale)
//   print_scaled(word, x, y, faceCol, scale)               // the face, last
//
// Everything's a live knob so you can dial the feel. A later slice swaps the
// bitmap font for a baked TTF display face (highnoon / tools/font-bake.js).

#include "studio.h"

#define STATE struct GameState
#define S     ((STATE *)de_state(sizeof(STATE)))

STATE {
    int  depth;      // how many extrude copies
    int  dx, dy;     // per-copy offset vector
    int  scale;      // print_scaled scale of the face
    bool ramp;       // darken each layer toward the back?
    int  word;       // which demo string
    int  pal;        // which face/back colour pair
};

static const char *WORDS[] = { "GET READY", "PUSH START", "HOTLINE", "DREAMENGINE", "1985" };
#define NWORDS ((int)(sizeof(WORDS)/sizeof(WORDS[0])))

// face colour, back (extrude) colour — flat pairs plus the ramp start point
static const int FACE[] = { CLR_WHITE, CLR_YELLOW, CLR_PINK,       CLR_GREEN  };
static const int BACK[] = { CLR_PINK,  CLR_RED,    CLR_DARK_PURPLE, CLR_DARK_GREEN };
#define NPAL ((int)(sizeof(FACE)/sizeof(FACE[0])))

void init(void) {
    S->depth = 8;
    S->dx = 1; S->dy = 1;
    S->scale = 4;
    S->ramp = false;
    S->word = 0;
    S->pal = 2;      // pink-on-purple, the Miami default
}

void update(void) {
    if (btnp(0, BTN_UP))    S->depth++;
    if (btnp(0, BTN_DOWN) && S->depth > 0) S->depth--;

    // shift the extrude vector — arrows nudge x, brackets nudge y
    if (btnp(0, BTN_LEFT))  S->dx--;
    if (btnp(0, BTN_RIGHT)) S->dx++;
    if (keyp('[')) S->dy--;
    if (keyp(']')) S->dy++;
    if (S->dx < -3) S->dx = -3; if (S->dx > 3) S->dx = 3;
    if (S->dy < -3) S->dy = -3; if (S->dy > 3) S->dy = 3;

    if (btnp(0, BTN_A)) S->ramp = !S->ramp;                 // Z
    if (btnp(0, BTN_B)) S->word = (S->word + 1) % NWORDS;   // X
    if (keyp('c') || keyp('C')) S->pal = (S->pal + 1) % NPAL;
}

// darken a layer toward the back: lerp its index t=0..1 down toward CLR_BLACK-ish.
// The palette isn't a smooth ramp, so we fake depth by stepping through a couple
// of darker indices near the back colour — good enough to read as a bevel.
static int layer_col(int back, int i, int n, bool ramp) {
    if (!ramp || n <= 1) return back;
    // back third stays `back`; the deepest copies drop to black for a bevel
    float t = (float)i / (float)n;          // 0 = nearest face, 1 = deepest
    if (t > 0.66f) return CLR_BLACK;
    if (t > 0.33f) return CLR_DARKER_PURPLE;
    return back;
}

void draw(void) {
    cls(CLR_DARKER_BLUE);

    const char *word = WORDS[S->word];
    int face = FACE[S->pal], back = BACK[S->pal];

    int w  = text_width(word) * S->scale;
    int th = 8 * S->scale;                             // 8px font cell
    // centre the FACE; the extrude grows down-right from it
    int fx = (SCREEN_W - w) / 2;
    int fy = (SCREEN_H - th) / 2 - (S->depth * S->dy) / 2;

    // back-to-front: deepest copy first, face last on top
    for (int i = S->depth; i >= 1; i--) {
        int c = layer_col(back, i, S->depth, S->ramp);
        print_scaled(word, fx + i * S->dx, fy + i * S->dy, c, S->scale);
    }
    print_scaled(word, fx, fy, face, S->scale);

    // ── knob readout ──
    font(FONT_SMALL);
    int y = 4;
    print(str("depth %d  (UP/DOWN)", S->depth), 4, y, CLR_LIGHT_GREY); y += 8;
    print(str("offset %+d,%+d  (LEFT/RIGHT, [ ])", S->dx, S->dy), 4, y, CLR_LIGHT_GREY); y += 8;
    print(str("ramp %s  (Z)", S->ramp ? "on" : "off"), 4, y, CLR_LIGHT_GREY); y += 8;
    print("word X   palette C", 4, y, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);
}
